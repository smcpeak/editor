// rfa-async-single-client.h
// RFA_AsyncSingleClient class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_ASYNC_SINGLE_CLIENT_H
#define EDITOR_RFA_ASYNC_SINGLE_CLIENT_H

#include "rfa-async-single-client-fwd.h"         // fwds for this module

// editor
#include "command-runner.h"                      // CommandRunner
#include "host-name.h"                           // HostName
#include "rfa-msg.h"                             // RFA_Message

// smbase
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/str.h"                          // string

// qt
#include <QByteArray>
#include <QObject>

// libc++
#include <memory>                                // std::unique_ptr

// libc
#include <stdint.h>                              // int64_t


// Class to issue asynchronous file system queries to a process that
// implements the RFA protocol.  The process could be locally serving
// the requests or an SSH process communicating to the real server on
// another machine.
class RFA_AsyncSingleClient : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(RFA_AsyncSingleClient);

public:      // types
  // States that the query object can be in.
  //
  // See doc/rfa-query-lifecycle.ded.png for the life cycle.
  enum State {
    S_CREATED,     // Just created.
    S_CONNECTING,  // Establishing connection.
    S_READY,       // Ready for requests.
    S_WAITING,     // Request sent, reply not received.
    S_HAS_REPLY,   // Reply received and available for client.
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
  std::unique_ptr<RFA_Message> m_replyMessage;

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
  void innerSendRequest(RFA_Message const &msg);

public:      // methods
  RFA_AsyncSingleClient();

  // Preferably, 'shutdown' should be explicitly called before the
  // destructor is.
  virtual ~RFA_AsyncSingleClient() noexcept;

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
  void sendRequest(RFA_Message const &msg);

  // Take the reply object.
  //
  // Requires: state() == S_HAS_REPLY
  std::unique_ptr<RFA_Message> takeReply();

  // Transition to the S_FAILED state with the given reason.
  //
  // This can be useful when a client sees an invalid response from the
  // server and wants to mark the connection as invalid.
  //
  // Calling this causes the 'signal_rfaFailureAvailable' signal to be
  // emitted (unless there was a prior failure; the signal is only sent
  // once).
  //
  // Requires: state() != S_DEAD
  void markAsFailed(string const &reason);

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
  void signal_rfaConnected();

  // Emitted when state() becomes S_HAS_REPLY.
  void signal_rfaReplyAvailable();

  // Emitted when state() becomes S_FAILED.
  void signal_rfaFailureAvailable();

protected Q_SLOTS:
  // Handlers for CommandRunner signals.
  void on_outputDataReady() NOEXCEPT;
  void on_errorDataReady() NOEXCEPT;
  void on_processTerminated() NOEXCEPT;
};


// Returns a string like "S_READY".
char const *toString(RFA_AsyncSingleClient::State s);


#endif // EDITOR_RFA_ASYNC_SINGLE_CLIENT_H
