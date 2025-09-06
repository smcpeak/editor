// json-rpc-client.h
// Client for JSON-RPC 2.0, which is the base layer for Language Server
// Protocol.

#ifndef EDITOR_JSON_RPC_CLIENT_H
#define EDITOR_JSON_RPC_CLIENT_H

#include "json-rpc-client-fwd.h"       // fwds for this module

#include "command-runner-fwd.h"        // CommandRunner

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // NULLABLE
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QObject>

#include <iosfwd>                      // std::ostream
#include <list>                        // std::list
#include <map>                         // std::map
#include <optional>                    // std::optional
#include <set>                         // std::set
#include <string>                      // std::string
#include <string_view>                 // std::string_view
#include <vector>                      // std::vector


// Manage communication with a child process that is a JSON-RPC server
// communicating over stdin and stdout.
class JSON_RPC_Client : public QObject {
  Q_OBJECT

private:     // types
  // Possible outcomes when attempting to parse the accumulated output
  // data as a message.  These only the cover the cases of explicit
  // returns from `innerProcessOutputData`, which mostly happen when the
  // parser reaches the end of the available data without finding a
  // complete message, which is only an error if the client terminated
  // too.  The parsing attempt will throw `XFormat` if the data that
  // *has* been received is problematic.
  enum MessageParseResult {
    // We successfully extracted one message.
    MPR_ONE_MESSAGE,

    // The remaining cases all correspond to an inability to extract a
    // message.

    // We made no attempt to parse due to a prior protocol error.
    MPR_PRIOR_ERROR,

    // There was no data, so no message.
    MPR_EMPTY,

    // The headers did not have a blank line terminator.
    MPR_UNTERMINATED_HEADERS,

    // A header line lacks a newline.
    MPR_UNTERMINATED_HEADER_LINE,

    // The available data is less than the Content-Length indicated.
    MPR_INCOMPLETE_BODY,

    NUM_MESSAGE_PARSE_RESULTS
  };

private:     // data
  // Object managing byte-level communication with the child.
  CommandRunner &m_child;

  // If something goes wrong on the protocol level, debugging details
  // will be logged here.  If it is null, those details will just be
  // discarded.
  std::ostream * NULLABLE m_protocolDiagnosticLog;

  // The ID to use for the next request we send.  Always positive.
  int m_nextRequestID;

  // IDs of requests that have been sent but for which no reply has
  // been received.
  std::set<int> m_outstandingRequests;

  // Map from ID to received reply that has not yet been taken by the
  // client.
  //
  // Invariant: The key set of this map is disjoint with
  // `m_outstandingRequests`.
  std::map<int, gdv::GDValue> m_pendingReplies;

  // Set of IDs of requests that have been canceled, but for which we
  // have not seen the reply yet.
  //
  // Invariant: This set is disjoint with both `m_outstandingRequests`
  // and the key set of `m_pendingReplies`.
  std::set<int> m_canceledRequests;

  // Sequence of received notifications, in chronological order, oldest
  // first, that have not been taken by the client.
  std::list<gdv::GDValue> m_pendingNotifications;

  // If set, this describes a protocol error that has happened, which
  // makes further communication with the child impossible.
  std::optional<std::string> m_protocolError;

private:     // methods
  // Return a string describing `res`.
  static char const *toString(MessageParseResult res);

  // Return the request ID to use for the next request, and also
  // increment `m_nextRequestID`, wrapping when necessary.
  int innerGetNextRequestID();

  // Like `inner`, except skip any IDs already in use.
  int getNextRequestID();

  // Send `data` to the child process stdn.
  void send(std::string &&data);

  // Turn `msg` into a sequence of bytes to send.
  static std::string serializeMessage(gdv::GDValue const &msg);

  // Construct a sequence of bytes to represent a request.
  static std::string makeRequest(
    int id,
    std::string_view method,
    gdv::GDValue const &params);

  // Construct a notification body (excluding headers) byte sequence.
  static std::string makeNotificationBody(
    std::string_view method,
    gdv::GDValue const &params);

  // If `m_protocolError` is currently nullopt, set it to `msg` and emit
  // the appropriate signal.  Otherwise do nothing.
  void setProtocolError(std::string &&msg);

  // Parse `bodyJSON` into GDV.
  gdv::GDValue call_jsonToGDV(std::string const &bodyJSON) const;

  // Attempt to parse the current output data as a message.  If
  // successful, remove the message data from the queue, add an entry to
  // `m_pendingReplies` or `m_pendingNotifications` as appropriate for
  // the message, then return `MPR_ONE_MESSAGE`.  In this case, there
  // might still be more messages in the data queue, so this function
  // should be called in a loop.
  //
  // If there is not enough data, return an appropriate result code
  // (which will only become relevant if the child process terminates
  // while the data is still incomplete), and don't change anything,
  // since the expectation is this function will be called again once
  // more data arrives.
  //
  // If there is an error with data that *has* been received, throw
  // `XFormat`.
  MessageParseResult innerProcessOutputData();

private Q_SLOTS:
  // Process queued data in `m_child`.
  void processOutputData() NOEXCEPT;

  // Respond to the similarly-named `CommandRunner` signals.
  void on_errorDataReady() NOEXCEPT;
  void on_processTerminated() NOEXCEPT;

public:      // methods
  ~JSON_RPC_Client();

  // Begin communicating using LSP with `child`, a process that has
  // already been started.
  explicit JSON_RPC_Client(
    CommandRunner &child,
    std::ostream * NULLABLE protocolDiagnosticLog);

  // Assert invariants.
  void selfCheck() const;

  // Send a notification.  Do not wait for any responses.
  //
  // Requires `!hasProtocolError()`.
  void sendNotification(
    std::string_view method,
    gdv::GDValue const &params);

  // True if there are pending notifications to dequeue.
  bool hasPendingNotifications() const;

  // Number of pending notifications.
  int numPendingNotifications() const;

  // Return the oldest notification that has not been dequeued, removing
  // it from those that are pending.  Requires
  // `hasPendingNotifications()`.
  gdv::GDValue takeNextNotification();

  // Send a request.  Return its ID, which can be used to correlate with
  // the eventual reply.
  //
  // Requires `!hasProtocolError()`.
  //
  // Ensures `return > 0`.  Consequently, a client can safely use a 0 ID
  // to mean "absent".
  //
  int sendRequest(
    std::string_view method,
    gdv::GDValue const &params);

  // True if we have received a reply for request `id`.
  bool hasReplyForID(int id) const;

  // Return the set of IDs of requests that have been sent to the server
  // but for which no reply has been received.
  std::set<int> getOutstandingRequestIDs() const;

  // Return the set of IDs of replies that have been received but not
  // yet taken from this object.
  std::set<int> getPendingReplyIDs() const;

  // Return the reply for `id`.  Requires `hasReplyForID(id)`.
  //
  // This returns the value of the "result" field of the entire reply.
  gdv::GDValue takeReplyForID(int id);

  // If the reply for `id` is pending, discard it.  If not, arrange to
  // discard it when it arrives.
  //
  // TODO: Send a cancelation to the server.
  void cancelRequestWithID(int id);

  // True if a protocol error has occurred.  In this state, no further
  // messages can be sent or received.
  bool hasProtocolError() const;

  // Requires `hasProtocolError()`.  Get the messages.
  std::string getProtocolError() const;

  // True if the child process is still running.
  bool isChildRunning() const;

  // True if there is some data on stderr.
  bool hasErrorData() const;

  // Take the stderr data.  Returns an empty array if there is none.
  QByteArray takeErrorData();

  // ----------------------------- signals -----------------------------
Q_SIGNALS:
  // Emitted when `hasPendingNotifications()` becomes true.
  void signal_hasPendingNotifications();

  // Emitted when `hasReplyForID(id)` becomes true.
  void signal_hasReplyForID(int id);

  // Emitted when `hasProtocolError()` becomes true.
  void signal_hasProtocolError();

  // Emitted when `isChildRunning()` becomes false.
  //
  // If the child terminates without sending a complete message,
  // `signal_hasProtocolError()` fires first, followed by this signal.
  //
  void signal_childProcessTerminated();

  // Emitted when error data is received, thus `hasErrorData()` is true
  // and may have just become true (but might have already been).
  void signal_hasErrorData();
};


#endif // EDITOR_JSON_RPC_CLIENT_H
