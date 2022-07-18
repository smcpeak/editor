// vfs-query.cc
// Code for vfs-query.h.

#include "vfs-query.h"                 // this module

// editor
#include "waiting-counter.h"           // adjWaitingCounter

// smqtutil
#include "qtutil.h"                    // toString(QString), printQByteArray

// smbase
#include "bflatten.h"                  // StreamFlatten
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "flatten.h"                   // serializeIntNBO
#include "nonport.h"                   // getFileModificationTime
#include "overflow.h"                  // convertWithoutLoss
#include "sm-macros.h"                 // DEFINE_ENUMERATION_TO_STRING
#include "trace.h"                     // TRACE

// qt
#include <QCoreApplication>

// libc++
#include <sstream>                     // std::i/ostringstream
#include <utility>                     // std::move

// libc
#include <string.h>                    // memcpy


FileSystemQuery::FileSystemQuery()
  : QObject(),
    m_state(S_CREATED),
    m_hostname(),
    m_commandRunner(),
    m_replyBytes(),
    m_errorBytes(),
    m_replyMessage(),
    m_failureReason()
{
  QObject::connect(&m_commandRunner, &CommandRunner::signal_outputDataReady,
                   this, &FileSystemQuery::on_outputDataReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_errorDataReady,
                   this, &FileSystemQuery::on_errorDataReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_processTerminated,
                   this, &FileSystemQuery::on_processTerminated);
}


FileSystemQuery::~FileSystemQuery()
{
  // See doc/signals-and-dtors.txt.
  disconnectSignals();
}


static bool isWaitingState(FileSystemQuery::State s)
{
  return s == FileSystemQuery::S_CONNECTING ||
         s == FileSystemQuery::S_PENDING;
}


void FileSystemQuery::setState(State s)
{
  TRACE("FileSystemQuery",
    "setState: " << toString(m_state) << " -> " << toString(s));

  // Inform 'EventReplay' if our waitingness has changed.
  int wasWaiting = isWaitingState(m_state);
  int nowWaiting = isWaitingState(s);
  adjWaitingCounter(nowWaiting - wasWaiting);

  m_state = s;
}


void FileSystemQuery::disconnectSignals()
{
  QObject::disconnect(&m_commandRunner, nullptr, this, nullptr);
}


void FileSystemQuery::recordFailure(string const &reason)
{
  if (state() != S_FAILED) {
    setState(S_FAILED);
    m_failureReason = reason;
    Q_EMIT signal_failureAvailable();
  }
  else {
    TRACE("FileSystemQuery",
      "recordFailure: discarding additional reason: " << reason);
  }
}


void FileSystemQuery::checkForCompleteReply()
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

  if (tracingSys("FileSystemQuery_detail")) {
    printQByteArray(m_replyBytes, "reply bytes");
  }

  std::string messageBytes(m_replyBytes.data()+4, replyLen);
  std::istringstream iss(messageBytes);
  StreamFlatten flat(&iss);
  m_replyMessage.reset(VFS_Message::deserialize(flat));

  TRACE("FileSystemQuery",
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
        TRACE("FileSystemQuery", "confirmed protocol compatibility");
        setState(S_READY);
        m_replyMessage.reset(nullptr);
        Q_EMIT signal_connected();
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
    Q_EMIT signal_replyAvailable();
  }
}


void FileSystemQuery::connect(string hostname)
{
  xassert(state() == S_CREATED);

  setState(S_CONNECTING);
  m_hostname = hostname;

  if (hostname.empty()) {
    m_commandRunner.setProgram(
      QCoreApplication::applicationDirPath() + "/editor-fs-server.exe");
  }
  else {
    // Assume 'ssh' is on the local PATH.
    m_commandRunner.setProgram("ssh");

    m_commandRunner.setArguments(QStringList{
      // Force SSH to never prompt for a password.  Instead, just fail
      // if it cannot log in without prompting.
      "-oBatchMode=yes",

      toQString(m_hostname),

      // This requires that 'editor-fs-server.exe' be found on the
      // user's PATH on the remote machine.
      //
      // It is not necessary to disable the SSH escape character
      // because, by passing the name of a program, the SSH session is
      // not considered "interactive", and hence by default does not
      // create a PTY, which is itself a prerequisite to escape
      // character recognition.
      "editor-fs-server.exe"
    });
  }

  TRACE("FileSystemQuery",
    "starting command: " << toString(m_commandRunner.getCommandLine()));
  m_commandRunner.startAsynchronous();

  // Attempt to establish version compatibility.
  VFS_GetVersion getVer;
  innerSendRequest(getVer);
}


string FileSystemQuery::getHostname() const
{
  return m_hostname;
}


void FileSystemQuery::innerSendRequest(VFS_Message const &msg)
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
  TRACE("FileSystemQuery",
    "sending message: type=" << toString(msg.messageType()) <<
    " len=" << serMsgLen);
  if (tracingSys("FileSystemQuery_detail")) {
    printQByteArray(envelope, "envelope bytes");
  }
  m_commandRunner.putInputData(envelope);
}


void FileSystemQuery::sendRequest(VFS_Message const &msg)
{
  xassert(state() == S_READY);

  innerSendRequest(msg);

  setState(S_PENDING);
}


std::unique_ptr<VFS_Message> FileSystemQuery::takeReply()
{
  xassert(state() == S_HAS_REPLY);
  setState(S_READY);
  return std::move(m_replyMessage);
}


string FileSystemQuery::getFailureReason()
{
  xassert(state() == S_FAILED);
  return m_failureReason;
}


void FileSystemQuery::shutdown()
{
  disconnectSignals();
  setState(S_DEAD);
  m_commandRunner.closeInputChannel();
  m_commandRunner.killProcess();
}


void FileSystemQuery::on_outputDataReady() NOEXCEPT
{
  TRACE("FileSystemQuery", "on_outputDataReady");

  m_replyBytes.append(m_commandRunner.takeOutputData());
  checkForCompleteReply();
}

void FileSystemQuery::on_errorDataReady() NOEXCEPT
{
  TRACE("FileSystemQuery", "on_errorDataReady");

  // Just accumulate the error bytes.  We'll deal with them when
  // looking at output data or process termination.
  m_errorBytes.append(m_commandRunner.takeErrorData());
}

void FileSystemQuery::on_processTerminated() NOEXCEPT
{
  TRACE("FileSystemQuery", "on_processTerminated");

  // An uncommanded termination is an error.  (And a commanded
  // termination is preceded by disconnecting signals, so we would not
  // get here.)
  stringBuilder sb;
  sb << "editor-fs-server terminated unexpectedly: ";
  sb << toString(m_commandRunner.getTerminationDescription());
  if (!m_errorBytes.isEmpty()) {
    sb << "  stderr: " << m_errorBytes.toStdString();
  }
  recordFailure(sb.str());
}


DEFINE_ENUMERATION_TO_STRING(
  FileSystemQuery::State,
  FileSystemQuery::NUM_STATES,
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
