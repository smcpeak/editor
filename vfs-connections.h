// vfs-connections.h
// VFS_Connections class.

#ifndef EDITOR_VFS_CONNECTIONS_H
#define EDITOR_VFS_CONNECTIONS_H

#include "vfs-connections-fwd.h"       // fwds for this module

// editor
#include "host-name.h"                 // HostName
#include "vfs-msg.h"                   // VFS_Message
#include "vfs-query-fwd.h"             // FileSystemQuery

// smbase
#include "sm-macros.h"                 // NO_OBJECT_COPIES

// qt
#include <QObject>

// libc++
#include <list>                        // std::list
#include <map>                         // std::map
#include <memory>                      // std::unique_ptr
#include <utility>                     // std::move
#include <vector>                      // std::vector


// Collection of active FileSystemQuery objects, and an asynchronous
// query interface on top of them.
class VFS_Connections : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_Connections);

public:      // types
  // Unique identifier for a request.  Never zero for a valid request.
  typedef uint64_t RequestID;

private:     // types
  // A request that has been issued by a client but not yet sent on any
  // connection.
  class QueuedRequest {
  public:      // data
    // This request's ID.
    RequestID m_requestID;

    // The request object.
    std::unique_ptr<VFS_Message> m_requestObject;

  public:      // methods
    QueuedRequest(RequestID requestID,
                  std::unique_ptr<VFS_Message> requestObject)
      : m_requestID(requestID),
        m_requestObject(std::move(requestObject))
    {}
  };

  // Information related to the connection to a specific host.
  class Connection {
  public:      // data
    // Owning set of connections.
    VFS_Connections *m_connections;

    // The host.
    HostName m_hostName;

    // The query object.
    std::unique_ptr<FileSystemQuery> m_fsQuery;

    // ID of the request currently being serviced by 'm_fsQuery', or 0
    // if none is.
    RequestID m_currentRequestID;

    // Sequence of queued requests to send.
    std::list<QueuedRequest> m_queuedRequests;

  public:      // methods
    Connection(VFS_Connections *connections, HostName const &hostName);
    ~Connection();

    // True if 'requestID' has not received its reply.
    bool requestIsPending(RequestID requestID) const;

    // If 'm_fsQuery' is ready, and there are queued requests, send the
    // next one.
    void issuePendingRequest();

    // Cancel issuing and/or delivering reply for 'requestID'.  Return
    // true if the ID was found and canceled.
    bool cancelRequest(RequestID requestID);
  };

private:     // data
  // Next ID to assign to a request.
  RequestID m_nextRequestID;

  // Sequence of valid host names, in 'connect' order.
  std::vector<HostName> m_validHostNames;

  // Map from a name to its connection info.
  //
  // Invariant: The set of keys in 'm_connections' is equal to the set
  // of values in 'm_validHostNames'.
  //
  // TODO: I have map-with-vector.h in 'inst' that could combine the map
  // and vector, but it needs to be moved to smbase, and it needs to
  // have the capability of removing items.
  std::map<HostName, std::unique_ptr<Connection> > m_connections;

  // Map from request ID to the corresponding reply object, for those
  // requests whose reply is available.
  std::map<RequestID, std::unique_ptr<VFS_Message> > m_availableReplies;

private:      // methods
  // Get the connection for the given hostName.
  //
  // Requires: isValid(hostName)
  Connection const *connC(HostName const &hostName) const;
  Connection *conn(HostName const &hostName)
    { return const_cast<Connection*>(connC(hostName)); }

  // Assuming we received a signal from a FileSystemQuery, find and
  // return the Connection associated with the sender.  Return nullptr
  // if one cannot be found.
  Connection * NULLABLE signalRecipientConnection();

public:      // methods
  VFS_Connections();
  virtual ~VFS_Connections() override;

  // Throw an exception if an invariant does not hold.
  void selfCheck() const;

  // True if 'connect(hostName)' has been called and
  // 'shutdown(hostName)' has not.
  bool isValid(HostName const &hostName) const;

  // Get the set of HostNames for which 'isValid' is true, in the order
  // in which 'connect' was called.
  std::vector<HostName> getHostNames() const;

  // Connect to 'hostName'.
  //
  // Requires: !isValid(hostName)
  //
  // Ensures: isValid(hostName)
  void connect(HostName const &hostName);

  void connectLocal() { connect(HostName::asLocal()); }

  // True while we are setting up the connection to 'hostName'.
  //
  // Requires: isValid(hostName)
  bool isConnecting(HostName const &hostName) const;

  bool localIsConnecting() const { return isConnecting(HostName::asLocal()); }

  // True when 'hostName' is ready to receive requests.  This is true
  // even if a request is being processed, so long we can enqueue
  // another.
  //
  // Requires: isValid(hostName)
  bool isReady(HostName const &hostName) const;

  bool localIsReady() const { return isReady(HostName::asLocal()); }

  // Issue new request 'req'.  The request ID is stored in 'requestID'
  // *before* anything is done that might lead to delivery of the
  // reply.
  //
  // Requires: isValid(hostName)
  void issueRequest(RequestID /*OUT*/ &requestID,
                    HostName const &hostName,
                    std::unique_ptr<VFS_Message> req);

  // True if the given request is being processed.  False once the reply
  // is available.
  bool requestIsPending(RequestID requestID) const;

  // True if the reply to the indicated request is available.
  bool replyIsAvailable(RequestID requestID) const;

  // Retrieve the reply for the given request, removing it from the set
  // of pending replies.
  //
  // Requires: replyIsAvailable(requestID)
  std::unique_ptr<VFS_Message> takeReply(RequestID requestID);

  // Stop delivery of 'signal_replyArrived' for the named request if it
  // has not already been delivered.  Remove it from
  // 'm_availableReplies' if it is present there.
  void cancelRequest(RequestID requestID);

  // Shut down connection to 'hostName' and remove it from the set of
  // valid hosts.
  //
  // Requires: isValid(hostName)
  //
  // Ensures: !isValid(hostName)
  void shutdown(HostName const &hostName);

  // Shut down all connections.
  void shutdownAll();

  // True if the connection was lost.
  //
  // Requires: isValid(hostName)
  bool connectionWasLost(HostName const &hostName) const;

Q_SIGNALS:
  // Emitted when 'isConnecting()' transitions to 'isReady()'.
  void signal_connected(HostName hostName);

  // Emitted when 'replyIsAvailable(requestID)' becomes true.
  void signal_replyAvailable(RequestID requestID);

  // Emitted when 'connectionWasLost' becomes true.
  void signal_vfsConnectionLost(HostName hostName, string reason);

protected Q_SLOTS:
  // Handlers for FileSystemQuery.
  void on_connected() NOEXCEPT;
  void on_replyAvailable() NOEXCEPT;
  void on_failureAvailable() NOEXCEPT;
};


#endif // EDITOR_VFS_CONNECTIONS_H
