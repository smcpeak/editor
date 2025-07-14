// lsp-client.h
// Client for the Language Server Protocol.

#ifndef EDITOR_LSP_CLIENT_H
#define EDITOR_LSP_CLIENT_H

#include "command-runner-fwd.h"        // CommandRunner

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QObject>

#include <list>                        // std::list
#include <map>                         // std::map
#include <optional>                    // std::optional
#include <set>                         // std::set
#include <string>                      // std::string
#include <string_view>                 // std::string_view
#include <vector>                      // std::vector


// Manage communication with a child process that is an LSP server.
class LSPClient : public QObject {
  Q_OBJECT

private:     // data
  // Object managing byte-level communication with the child.
  CommandRunner &m_child;

  // The ID to use for the next request we send.
  int m_nextRequestID;

  // IDs of requests that have been sent but for which no reply has
  // been received.
  std::set<int> m_outstandingRequests;

  // Map from ID to received reply that has not yet been taken by the
  // client.
  std::map<int, gdv::GDValue> m_pendingReplies;

  // Sequence of received notifications, in chronological order, oldest
  // first, that have not been taken by the client.
  std::list<gdv::GDValue> m_pendingNotifications;

  // If set, this describes a protocol error that has happened, which
  // makes further communication with the child impossible.
  std::optional<std::string> m_protocolError;

private:     // methods
  // Return the request ID to use for the next request, and also
  // increment `m_nextRequestID`, wrapping when necessary.
  int innerGetNextRequestID();

  // Like `inner`, except skip any IDs already in use.
  int getNextRequestID();

  // True if `id` is either outstanding or pending.
  bool idInUse(int id) const;

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

  // Attempt to parse the current output data as a message.  If
  // successful, add an entry to `m_pendingReplies` or
  // `m_pendingNotifications` as appropriate for the message.  If there
  // is not enough data, just return without changing anything.
  //
  // If there is an error with the data format, throw an exception.
  void innerProcessOutputData();

private Q_SLOTS:
  // Process queued data in `m_child`.
  void processOutputData() NOEXCEPT;

public:      // methods
  ~LSPClient();

  // Begin communicating using LSP with `child`, a process that has
  // already been started.
  explicit LSPClient(CommandRunner &child);

  // Given a file name, convert that into a "file:" URI.
  static std::string makeFileURI(std::string_view fname);

  // Send a notification.  Do not wait for any responses.
  //
  // Requires `!hasProtocolError()`.
  void sendNotification(
    std::string_view method,
    gdv::GDValue const &params);

  // True if there are pending notifications to dequeue.
  bool hasPendingNotifications() const;

  // Return the oldest notification that has not been dequeued, removing
  // it from those that are pending.  Requires
  // `hasPendingNotifications()`.
  gdv::GDValue takeNextNotification();

  // Send a request.  Return its ID, which can be used to correlate with
  // the eventual reply.
  //
  // Requires `!hasProtocolError()`.
  int sendRequest(
    std::string_view method,
    gdv::GDValue const &params);

  // True if we have received a reply for request `id`.
  bool hasReplyForID(int id) const;

  // Return the reply for `id`.  Requires `hasReplyForID(id)`.
  gdv::GDValue takeReplyForID(int id);

  // True if a protocol error has occurred.  In this state, no further
  // messages can be sent or received.
  bool hasProtocolError() const;

  // Requires `hasProtocolError()`.  Get the messages.
  std::string getProtocolError() const;
};


#endif // EDITOR_LSP_CLIENT_H
