// vfs-query.h
// FileSystemQuery class.

#ifndef EDITOR_VFS_QUERY_H
#define EDITOR_VFS_QUERY_H

#include "vfs-query-fwd.h"             // fwds for this module

// editor
#include "command-runner.h"            // CommandRunner
#include "host-name.h"                 // HostName
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
class FileSystemQuery : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(FileSystemQuery);

public:      // types
  // States that the query object can be in.
  //
  // See doc/vfs-query-lifecycle.ded.png for the life cycle.
  enum State {
    S_CREATED,     // Just created.
    S_CONNECTING,  // Establishing connection.
    S_READY,       // Ready for requests.
    S_PENDING,     // Request sent, reply not received.
    S_HAS_REPLY,   // Reply waiting.
    S_FAILED,      // A failure happened.
    S_DEAD,        // Connection was shut down.

    NUM_STATES     // Number of valid states.
  };

private:     // data
  // Current state.
  State m_state;

  // Host being accessed (which could be local).
  HostName m_hostName;

  // Runner connected to the server process.
  CommandRunner m_commandRunner;

  // Bytes of the reply received so far.
  QByteArray m_replyBytes;

  // Bytes of error message received so far.
  QByteArray m_errorBytes;

  // If not empty, the complete reply message that is available.
  std::unique_ptr<VFS_Message> m_replyMessage;

  // Human-readable string explaining the failure.
  string m_failureReason;

private:     // methods
  // Set 'm_state'.  This method uses TRACE to record the state
  // transition for debugging purposes.
  void setState(State s);

  // Disconnect signals going to this object's slots.
  void disconnectSignals();

  // Set 'm_failed' and 'm_failureReason' and emit the appropriate
  // signal, unless 'm_failed' is already true.
  void recordFailure(string const &reason);

  // Look at the received data so far and decide if we have received
  // enough to constitute a reply.  If so, emit signals, etc.
  void checkForCompleteReply();

  // Send 'msg', but without the state() manipulation that the public
  // 'sendRequest' does.
  void innerSendRequest(VFS_Message const &msg);

public:      // methods
  FileSystemQuery();

  virtual ~FileSystemQuery();

  // Get current state.
  State state() const { return m_state; }

  // Some convenient interpretations of 'state()'.
  bool isConnecting() const { return state() == S_CONNECTING; }
  bool isReady() const { return state() == S_READY; }
  bool hasFailed() const { return state() == S_FAILED; }
  bool hasReply() const { return state() == S_HAS_REPLY; }

  // Establish a connection to the given host, which can be the empty
  // string to indicate to access the local file system.
  //
  // Requires: state() == S_CREATED
  //
  // Ensures: state() == S_CONNECTING
  void connect(HostName const &hostname);

  void connectLocal() { connect(HostName::asLocal()); }

  // Get the host we are connecting to.
  HostName getHostName() const;

  // Send 'msg' to the server for processing.
  //
  // Requires: state() == S_READY
  void sendRequest(VFS_Message const &msg);

  // Take the reply object.
  //
  // Requires: state() == S_HAS_REPLY
  std::unique_ptr<VFS_Message> takeReply();

  // Get a string explaining the failure.  This will include any error
  // message bytes produced by the server.  The connection is dead after
  // any failure.
  //
  // Requires: state() == S_FAILED.
  string getFailureReason();

  // Attempt an orderly shutdown of the server.
  //
  // Ensures: state() == S_DEAD.
  void shutdown();

Q_SIGNALS:
  // Emitted when state() transitions from S_CONNECTING to S_READY.
  void signal_connected();

  // Emitted when state() becomes S_HAS_REPLY.
  void signal_replyAvailable();

  // Emitted when state() becomes S_FAILED.
  void signal_failureAvailable();

protected Q_SLOTS:
  // Handlers for CommandRunner signals.
  void on_outputDataReady() NOEXCEPT;
  void on_errorDataReady() NOEXCEPT;
  void on_processTerminated() NOEXCEPT;
};


// Returns a string like "S_READY".
char const *toString(FileSystemQuery::State s);


#endif // EDITOR_VFS_QUERY_H
