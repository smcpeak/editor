// vfs-query.cc
// Code for vfs-query.h.

#include "vfs-query.h"                 // this module

// editor
#include "waiting-counter.h"           // adjWaitingCounter

// smqtutil
#include "smqtutil/qtutil.h"           // toString(QString), printQByteArray

// smbase
#include "smbase/bflatten.h"           // StreamFlatten
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/flatten.h"            // serializeIntNBO
#include "smbase/nonport.h"            // getFileModificationTime
#include "smbase/overflow.h"           // convertWithoutLoss
#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING
#include "smbase/trace.h"              // TRACE

// qt
#include <QCoreApplication>

// libc++
#include <sstream>                     // std::i/ostringstream
#include <utility>                     // std::move

// libc
#include <string.h>                    // memcpy


VFS_FileSystemQuery::VFS_FileSystemQuery()
  : QObject(),
    m_state(S_CREATED),
    m_hostName(HostName::asLocal()),
    m_commandRunner(),
    m_replyBytes(),
    m_errorBytes(),
    m_replyMessage(),
    m_failureReason()
{
  QObject::connect(&m_commandRunner, &CommandRunner::signal_outputDataReady,
                   this, &VFS_FileSystemQuery::on_outputDataReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_errorDataReady,
                   this, &VFS_FileSystemQuery::on_errorDataReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_processTerminated,
                   this, &VFS_FileSystemQuery::on_processTerminated);
}


VFS_FileSystemQuery::~VFS_FileSystemQuery() noexcept
{
  GENERIC_CATCH_BEGIN

  // See doc/signals-and-dtors.txt.
  disconnectSignals();

  // Try to kill the server process since otherwise QProcess will print
  // a warning about it.
  if (m_commandRunner.isRunning()) {
    shutdown();
  }

  GENERIC_CATCH_END
}


static bool isWaitingState(VFS_FileSystemQuery::State s)
{
  return s == VFS_FileSystemQuery::S_CONNECTING ||
         s == VFS_FileSystemQuery::S_PENDING;
}


void VFS_FileSystemQuery::setState(State s)
{
  TRACE("VFS_FileSystemQuery",
    "setState: " << toString(m_state) << " -> " << toString(s));

  // Inform 'EventReplay' if our waitingness has changed.
  int wasWaiting = isWaitingState(m_state);
  int nowWaiting = isWaitingState(s);
  adjWaitingCounter(nowWaiting - wasWaiting);

  m_state = s;
}


void VFS_FileSystemQuery::disconnectSignals()
{
  QObject::disconnect(&m_commandRunner, nullptr, this, nullptr);
}


void VFS_FileSystemQuery::recordFailure(string const &reason)
{
  if (state() != S_FAILED) {
    setState(S_FAILED);
    m_failureReason = reason;
    Q_EMIT signal_vfsFailureAvailable();
  }
  else {
    TRACE("VFS_FileSystemQuery",
      "recordFailure: discarding additional reason: " << reason);
  }
}


void VFS_FileSystemQuery::checkForCompleteReply()
{
  uint32_t replyBytesSize = m_replyBytes.size();
  if (replyBytesSize < 4) {
    return;
  }

  unsigned char lenBuf[4];
  memcpy(lenBuf, m_replyBytes.data(), 4);
  uint32_t replyLen;
  deserializeIntNBO(lenBuf, replyLen);

  if (replyBytesSize < 4+replyLen) {
    return;
  }

  if (replyBytesSize > 4+replyLen) {
    recordFailure(stringb(
      "Reply has extra bytes.  Its size is " << replyBytesSize <<
      " but the expected size is " << (4+replyLen) << "."));
    return;
  }

  if (tracingSys("VFS_FileSystemQuery_detail")) {
    printQByteArray(m_replyBytes, "reply bytes");
  }

  std::string messageBytes(m_replyBytes.data()+4, replyLen);
  std::istringstream iss(messageBytes);
  StreamFlatten flat(&iss);
  m_replyMessage.reset(VFS_Message::deserialize(flat));

  TRACE("VFS_FileSystemQuery",
    "received reply: type=" << toString(m_replyMessage->messageType()) <<
    " len=" << replyLen);

  // Clear the reply bytes so we are ready for the next one.
  m_replyBytes.clear();

  // Having done all that, if we have error bytes, then regard that as
  // a protocol violation and switch over to the failure case.
  if (!m_errorBytes.isEmpty()) {
    recordFailure("Error bytes were present (along with a valid "
                  "reply, now discarded).");
    return;
  }

  if (state() == S_CONNECTING) {
    // Check that the reply is the version message.
    if (VFS_GetVersion const *getVer = m_replyMessage->ifGetVersionC()) {
      // Confirm compatibility.
      if (getVer->m_version == VFS_currentVersion) {
        // Good to go.
        TRACE("VFS_FileSystemQuery", "confirmed protocol compatibility");
        setState(S_READY);
        m_replyMessage.reset(nullptr);
        Q_EMIT signal_vfsConnected();
      }
      else {
        recordFailure(stringb(
          "fs-server reports version " << getVer->m_version <<
          " but this client uses version " << VFS_currentVersion <<
          "."));
        return;
      }
    }
    else {
      recordFailure(stringb(
        "Server replied with invalid message type: " <<
        m_replyMessage->messageType()));
      return;
    }
  }

  else {
    setState(S_HAS_REPLY);
    Q_EMIT signal_vfsReplyAvailable();
  }
}


void VFS_FileSystemQuery::connect(HostName const &hostname)
{
  xassert(state() == S_CREATED);

  setState(S_CONNECTING);
  m_hostName = hostname;

  if (m_hostName.isLocal()) {
    m_commandRunner.setProgram(
      QCoreApplication::applicationDirPath() + "/editor-fs-server.exe");
  }
  else {
    bool didSetProgram = false;

    // Sequence of arguments to pass to the first 'ssh'.
    QStringList args;

    // As an experiment, allow the host name to have multiple hosts
    // conneded with "->", which means to do a sequence of nested ssh
    // calls to hop from machine to machine.  At each step we assume
    // 'ssh' i on the PATH.
    QStringList hostPath =
      toQString(m_hostName.getSSHHostName()).split("->");

    for (QString name : hostPath) {
      // Assume 'ssh' is on the local PATH.
      if (!didSetProgram) {
        m_commandRunner.setProgram("ssh");
        didSetProgram = true;
      }
      else {
        args.append("ssh");
      }

      // Force SSH to never prompt for a password.  Instead, just fail
      // if it cannot log in without prompting.
      args.append("-oBatchMode=yes");

      args.append(name);
    }

    // This requires that 'editor-fs-server.exe' be found on the
    // user's PATH on the remote machine.
    //
    // It is not necessary to disable the SSH escape character
    // because, by passing the name of a program, the SSH session is
    // not considered "interactive", and hence by default does not
    // create a PTY, which is itself a prerequisite to escape
    // character recognition.
    args.append("editor-fs-server.exe");

    m_commandRunner.setArguments(args);
  }

  TRACE("VFS_FileSystemQuery",
    "starting command: " << toString(m_commandRunner.getCommandLine()));
  m_commandRunner.startAsynchronous();

  // Attempt to establish version compatibility.
  VFS_GetVersion getVer;
  innerSendRequest(getVer);
}


HostName VFS_FileSystemQuery::getHostName() const
{
  return m_hostName;
}


void VFS_FileSystemQuery::innerSendRequest(VFS_Message const &msg)
{
  // Serialize the message.
  std::string serMessage;
  {
    std::ostringstream oss;
    StreamFlatten flat(&oss);
    msg.serialize(flat);
    serMessage = oss.str();
  }

  // Get its length.
  unsigned char lenBuf[4];
  uint32_t serMsgLen;
  convertWithoutLoss(serMsgLen, serMessage.size());
  serializeIntNBO(lenBuf, serMsgLen);

  // Combine the length and serialized message into an envelope.
  QByteArray envelope(serMsgLen+4, 0);
  memcpy(envelope.data(), lenBuf, 4);
  memcpy(envelope.data()+4, serMessage.data(), serMsgLen);

  // Send that to the child process.
  TRACE("VFS_FileSystemQuery",
    "sending message: type=" << toString(msg.messageType()) <<
    " len=" << serMsgLen);
  if (tracingSys("VFS_FileSystemQuery_detail")) {
    printQByteArray(envelope, "envelope bytes");
  }
  m_commandRunner.putInputData(envelope);
}


void VFS_FileSystemQuery::sendRequest(VFS_Message const &msg)
{
  xassert(state() == S_READY);

  innerSendRequest(msg);

  setState(S_PENDING);
}


std::unique_ptr<VFS_Message> VFS_FileSystemQuery::takeReply()
{
  xassert(state() == S_HAS_REPLY);
  setState(S_READY);
  return std::move(m_replyMessage);
}


void VFS_FileSystemQuery::markAsFailed(string const &reason)
{
  xassert(state() != S_DEAD);
  recordFailure(reason);
}


string VFS_FileSystemQuery::getFailureReason()
{
  xassert(state() == S_FAILED);
  return m_failureReason;
}


void VFS_FileSystemQuery::shutdown()
{
  disconnectSignals();
  setState(S_DEAD);
  m_commandRunner.closeInputChannel();
  m_commandRunner.killProcess();
}


void VFS_FileSystemQuery::on_outputDataReady() NOEXCEPT
{
  TRACE("VFS_FileSystemQuery", "on_outputDataReady");

  m_replyBytes.append(m_commandRunner.takeOutputData());
  checkForCompleteReply();
}

void VFS_FileSystemQuery::on_errorDataReady() NOEXCEPT
{
  TRACE("VFS_FileSystemQuery", "on_errorDataReady");

  // Just accumulate the error bytes.  We'll deal with them when
  // looking at output data or process termination.
  m_errorBytes.append(m_commandRunner.takeErrorData());
}

void VFS_FileSystemQuery::on_processTerminated() NOEXCEPT
{
  TRACE("VFS_FileSystemQuery", "on_processTerminated");

  // An uncommanded termination is an error.  (And a commanded
  // termination is preceded by disconnecting signals, so we would not
  // get here.)
  std::ostringstream sb;
  sb << "editor-fs-server terminated unexpectedly: ";
  sb << toString(m_commandRunner.getTerminationDescription());
  if (!m_errorBytes.isEmpty()) {
    sb << "  stderr: " << m_errorBytes.toStdString();
  }
  recordFailure(sb.str());
}


DEFINE_ENUMERATION_TO_STRING(
  VFS_FileSystemQuery::State,
  VFS_FileSystemQuery::NUM_STATES,
  (
    "S_CREATED",
    "S_CONNECTING",
    "S_READY",
    "S_PENDING",
    "S_HAS_REPLY",
    "S_FAILED",
    "S_DEAD",
  )
)


// EOF
