// vfs-query.h
// FileSystemQuery class.

#ifndef EDITOR_VFS_QUERY_H
#define EDITOR_VFS_QUERY_H

// editor
#include "command-runner.h"            // CommandRunner
#include "vfs-msg.h"                   // VFS_Message

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "str.h"                       // string

// qt
#include <QByteArray>
#include <QObject>

// libc++
#include <memory>                      // std::unique_ptr

// libc
#include <stdint.h>                    // int64_t


// Class to issue asynchronous file system queries to a process that
// implements the VFS protocol.  The process could be locally serving
// the requests or an SSH process communicating to the real server on
// another machine.
//
// The basic usage cycle looks like this:
//
//   +-----------+
//   |   Ready   |<--------------------+
//   +-----------+                     |
//         |                           |
//         | sendRequest               |
//         V                           |
//   +-----------+                     |
//   |  Pending  |                     |
//   +-----------+                     |
//         |                           |
//         | signal_replyAvailable     |
//         V                           |
//   +-----------+    takeReply        |
//   |  hasReply |---------------------+
//   +-----------+
//
// An error or shutdown interrupts the cycle and leaves the object in an
// unusable state.
//
class FileSystemQuery : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(FileSystemQuery);

private:     // data
  // Runner connected to the server process.
  CommandRunner m_commandRunner;

  // True when a request has been sent, but a complete reply has not
  // been received.
  bool m_waitingForReply;

  // Bytes of the reply received so far.
  QByteArray m_replyBytes;

  // Bytes of error message received so far.
  QByteArray m_errorBytes;

  // If not empty, the complete reply message that is available.
  std::unique_ptr<VFS_Message> m_replyMessage;

  // If true, the server failed to produce a valid reply.
  bool m_failed;

  // Human-readable string explaining the failure.
  string m_failureReason;

private:     // methods
  // Disconnect signals going to this object's slots.
  void disconnectSignals();

  // Set 'm_failed' and 'm_failureReason' and emit the appropriate
  // signal, unless 'm_failed' is already true.
  void recordFailure(string const &reason);

  // Look at the received data so far and decide if we have received
  // enough to constitute a reply.  If so, emit signals, etc.
  void checkForCompleteReply();

public:      // methods
  // Make an object to query the local file system.
  FileSystemQuery();

  virtual ~FileSystemQuery();

  // Send 'msg' to the server for processing.
  //
  // Requires '!hasPendingRequest()'.
  void sendRequest(VFS_Message const &msg);

  // True when a request has been sent but its reply has not been
  // received and taken.
  bool hasPendingRequest() const;

  // True when a reply is available and ready to be taken.
  //
  // Implies 'hasPendingRequest()'.
  bool hasReply() const;

  // Take the reply object.
  //
  // Requires 'hasReply()'.
  //
  // Ensures '!hasPendingRequest()'.
  std::unique_ptr<VFS_Message> takeReply();

  // True when the server failed to produce a valid reply.
  bool hasFailed() const;

  // Get a string explaining the failure.  This will include any error
  // message bytes produced by the server.  The connection is dead after
  // any failure.
  //
  // Requires 'hasFailed()'.
  string getFailureReason();

  // Attempt an orderly shutdown of the server.  After doing this, the
  // only permissible action is to destroy this object.
  void shutdown();

Q_SIGNALS:
  // Emitted when 'hasReply()' becomes true.
  void signal_replyAvailable();

  // Emitted when 'hasFailed()' becomes true.
  void signal_failureAvailable();

protected Q_SLOTS:
  // Handlers for CommandRunner signals.
  void on_outputDataReady() NOEXCEPT;
  void on_errorDataReady() NOEXCEPT;
  void on_processTerminated() NOEXCEPT;
};

#endif // EDITOR_VFS_QUERY_H
