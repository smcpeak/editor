// vfs-query.cc
// Code for vfs-query.h.

#include "vfs-query.h"                 // this module

// smqtutil
#include "qtutil.h"                    // toString(QString), printQByteArray

// smbase
#include "bflatten.h"                  // StreamFlatten
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "flatten.h"                   // serializeIntNBO
#include "nonport.h"                   // getFileModificationTime
#include "overflow.h"                  // convertWithoutLoss
#include "trace.h"                     // TRACE

// libc++
#include <sstream>                     // std::i/ostringstream
#include <utility>                     // std::move

// libc
#include <string.h>                    // memcpy


FileSystemQuery::FileSystemQuery()
  : QObject(),
    m_hostname(),
    m_commandRunner(),
    m_waitingForReply(false),
    m_replyBytes(),
    m_errorBytes(),
    m_replyMessage(),
    m_failed(false),
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


void FileSystemQuery::disconnectSignals()
{
  QObject::disconnect(&m_commandRunner, nullptr, this, nullptr);
}


void FileSystemQuery::recordFailure(string const &reason)
{
  if (!m_failed) {
    m_failed = true;
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

  // We now have a complete reply.
  m_waitingForReply = false;

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

  // Clear the reply bytes so we are ready for the next one.
  m_replyBytes.clear();

  // Having done all that, if we have error bytes, then regard that as
  // a protocol violation and switch over to the failure case.
  if (!m_errorBytes.isEmpty()) {
    recordFailure("Error bytes were present (along with a valid "
                  "reply, now discarded).");
  }

  else {
    Q_EMIT signal_replyAvailable();
  }
}


void FileSystemQuery::connect(string hostname)
{
  xassert(!wasConnected());

  m_wasConnected = true;
  m_hostname = hostname;

  if (hostname.empty()) {
    m_commandRunner.setProgram("./editor-fs-server.exe");
  }
  else {
    m_commandRunner.setProgram("ssh");

    // This requires that 'editor-fs-server.exe' be found on the user's
    // PATH on the remote machine.
    //
    // It is not necessary to disable the SSH escape character because,
    // by passing the name of a program, the SSH session is not
    // considered "interactive", and hence by default does not create a
    // PTY, which is itself a prerequisite to escape character
    // recognition.
    m_commandRunner.setArguments(QStringList{
      toQString(m_hostname),
      "editor-fs-server.exe"
    });
  }

  TRACE("FileSystemQuery",
    "starting command: " << toString(m_commandRunner.getCommandLine()));
  m_commandRunner.startAsynchronous();
}


string FileSystemQuery::getHostname() const
{
  xassert(wasConnected());
  return m_hostname;
}


void FileSystemQuery::sendRequest(VFS_Message const &msg)
{
  xassert(wasConnected() && !hasPendingRequest());

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
  TRACE("FileSystemQuery", "sending message, len=" << serMsgLen);
  if (tracingSys("FileSystemQuery_detail")) {
    printQByteArray(envelope, "envelope bytes");
  }
  m_commandRunner.putInputData(envelope);

  m_waitingForReply = true;
}


bool FileSystemQuery::hasPendingRequest() const
{
  return m_waitingForReply || m_replyMessage;
}


bool FileSystemQuery::hasReply() const
{
  return !!m_replyMessage;
}


std::unique_ptr<VFS_Message> FileSystemQuery::takeReply()
{
  xassert(hasReply());
  xassert(!m_waitingForReply);
  return std::move(m_replyMessage);
}


bool FileSystemQuery::hasFailed() const
{
  return m_failed;
}


string FileSystemQuery::getFailureReason()
{
  xassert(hasFailed());
  return m_failureReason;
}


void FileSystemQuery::shutdown()
{
  disconnectSignals();
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


// EOF
