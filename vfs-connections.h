// vfs-connections.h
// `VFS_Connections`, which accesses the VFS distributed virtual file
// system.

#ifndef EDITOR_VFS_CONNECTIONS_H
#define EDITOR_VFS_CONNECTIONS_H

#include "vfs-connections-fwd.h"       // fwds for this module

// editor
#include "host-name.h"                 // HostName
#include "vfs-msg.h"                   // VFS_Message
#include "vfs-query-fwd.h"             // VFS_FileSystemQuery

// smbase
#include "smbase/ordered-map-iface.h"  // smbase::OrderedMap
#include "smbase/refct-serf.h"         // SerfRefCount
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES

// qt
#include <QObject>

// libc++
#include <cstdint>                     // std::uint64_t
#include <list>                        // std::list
#include <map>                         // std::map
#include <memory>                      // std::unique_ptr
#include <string>                      // std::string
#include <utility>                     // std::move
#include <vector>                      // std::vector


/* Interface to a collection of VFS connections.  This is separated from
   `VFS_Connections` to facilitate testing; a test class can provide
   definitions of the virtual functions that avoids making actual IPC
   calls.

   This methods in this class (as opposed to `VFS_Connections`) provide:

     * Read-only queries for the state of host connections.

     * Read/write request and reply interaction.

   It thereby provides an interface that clients can use to access what
   is effectively a distributed virtual file system, but not to alter
   the set of hosts participating in that file system.

   From the perspective of this class, the request/reply lifecycle is:

     1. The request becomes "queued" as a result of `issueRequest`.

     2. The request is transmitted to the VFS server for the appropriate
        host, at which point we are "waiting" for the reply.  In states
        1 and 2 collectively, the request is "outstanding".  (A client
        of this class cannot distinguish between states 1 and 2.)

     3. The reply is received, making it "available".  The
        `signal_vfsReplyAvailable` fires upon this event.

     4. The reply is taken by `takeReply`, at which point its lifecycle
        for the purpose of this class ends.

   Note that I avoid the term "pending", as that can be ambiguous.
   Elsewhere (in the LSP client), "pending" means "available", but in
   the past I used it to mean "outstanding", and I'm trying to avoid
   re-creating that confusion.
*/
class VFS_AbstractConnections : public QObject,
                                public SerfRefCount {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_AbstractConnections);

public:      // types
  // Unique identifier for a request.  Never zero for a valid request.
  typedef std::uint64_t RequestID;

  // State of the connection to a single host.
  //
  // See doc/vfs-connections-lifecycle.ded.png for a transition
  // diagram.
  //
  // Note: These states have some similarities with
  // VFS_FileSystemQuery::State, but also important differences.  FSQ is
  // the lower-level module.
  enum ConnectionState {
    CS_INVALID,                        // Unknown host.
    CS_CONNECTING,                     // Trying to connect.
    CS_READY,                          // Ready for use.
    CS_FAILED_BEFORE_CONNECTING,       // Could not connect.
    CS_FAILED_AFTER_CONNECTING,        // Connected, then failed.

    NUM_CONNECTION_STATES
  };

protected:   // data
  // Next ID to assign to a request.
  RequestID m_nextRequestID;

  // Map from request ID to the corresponding reply object, for those
  // requests whose reply is available.
  std::map<RequestID, std::unique_ptr<VFS_Message> > m_availableReplies;

public:      // methods
  virtual ~VFS_AbstractConnections() override;

  // Initial state with no available replies.
  VFS_AbstractConnections();

  // Assert invariants.
  virtual void selfCheck() const;

  // --------------------------- Connections ---------------------------
  // Query the state of the connection to 'hostName'.
  //
  // All other methods in this section are defined in terms of this one.
  virtual ConnectionState connectionState(
    HostName const &hostName) const = 0;

  // True if `hostName` is a known host.
  bool isValid(HostName const &hostName) const
    { return connectionState(hostName) != CS_INVALID; }

  // True while we are setting up the connection to 'hostName'.
  bool isConnecting(HostName const &hostName) const
    { return connectionState(hostName) == CS_CONNECTING; }

  bool localIsConnecting() const { return isConnecting(HostName::asLocal()); }

  // True when 'hostName' is ready to receive requests and has not
  // failed.  This is true even if a request is being processed.
  //
  // Note that CS_READY can transition spontaneously to
  // CS_FAILED_AFTER_CONNECTING, so 'isReady' is never a method
  // precondition.
  bool isReady(HostName const &hostName) const
    { return connectionState(hostName) == CS_READY; }

  bool localIsReady() const { return isReady(HostName::asLocal()); }

  // True if we finished connecting to 'hostName' (and therefore became
  // 'isReady' and emitted 'signal_vfsConnected'), even if we then lost
  // the connection.
  bool isOrWasConnected(HostName const &hostName) const;

  // True if a connection was attempted, and possibly succeeded, but has
  // now failed, and 'shutdown' has not been called.
  bool connectionFailed(HostName const &hostName) const;

  // ---------------------- Requests and replies -----------------------
  // Issue new request 'req'.  The request ID is stored in 'requestID'
  // *before* anything is done that might lead to delivery of the
  // reply.
  //
  // Note that it *is* legal to call this while 'connectionFailed' (the
  // connection can be lost at any time, so the client would have no way
  // of reliably avoiding calling this in that state).  In that case,
  // the request will be effectively discarded, since it cannot be sent.
  //
  // It is also legal to call this while 'isConnecting'.  The request
  // will be enqueued and sent when the connection is ready.
  //
  // Requires: isValid(hostName)
  virtual void issueRequest(
    RequestID &requestID /*OUT*/,
    HostName const &hostName,
    std::unique_ptr<VFS_Message> req) = 0;

  // True if the given request is being processed.  False once the reply
  // is available.
  virtual bool requestIsOutstanding(RequestID requestID) const = 0;

  // True if the reply to the indicated request is available.
  bool replyIsAvailable(RequestID requestID) const;

  // Retrieve the reply for the given request, removing it from the set
  // of available replies.
  //
  // It is legal to call this even if the connection from which it
  // originated has failed or been shut down.
  //
  // Requires: replyIsAvailable(requestID)
  std::unique_ptr<VFS_Message> takeReply(RequestID requestID);

  // Stop delivery of 'signal_replyArrived' for the named request if it
  // has not already been delivered.  Remove it from
  // 'm_availableReplies' if it is present there.
  //
  // This does not interrupt processing of the request on the server, so
  // for example would not help if the server hangs (for that, one must
  // call 'shutdown' and then 'connect' again).  But it will prevent the
  // request from being sent if it is still queued, and causes the reply
  // to be discarded without notification if it is in flight.  It is
  // meant for cases where the client no longer needs the information
  // contained in the reply.
  virtual void cancelRequest(RequestID requestID) = 0;

  // Across all connections, the total number of requests that have been
  // issued (via 'issueRequest'), but whose reply has not arrived.  It
  // includes requests that are queued to be sent.  It does not count
  // requests that have been canceled.
  virtual int numOutstandingRequests() const = 0;

  // Total number of replies that are available but have not been taken
  // or canceled.
  int numAvailableReplies() const;

Q_SIGNALS:
  // Emitted when 'isConnecting(hostName)==true' transitions to
  // 'isReady(hostName)'.
  void signal_vfsConnected(HostName hostName);

  // Emitted when 'replyIsAvailable(requestID)' becomes true.
  void signal_vfsReplyAvailable(RequestID requestID);

  // Emitted when 'connectionFailed' becomes true.
  void signal_vfsFailed(HostName hostName, std::string reason);
};


// Returns a string like "CS_READY".
char const *toString(VFS_AbstractConnections::ConnectionState s);


// Collection of active VFS_FileSystemQuery objects, and an asynchronous
// query interface on top of them.
class VFS_Connections : public VFS_AbstractConnections {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_Connections);

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
    // Parent set of connections.  Never null.
    RCSerf<VFS_Connections> m_connections;

    // The host.
    HostName m_hostName;

    // The underlying query object.  Never null until teardown in the
    // destructor.
    std::unique_ptr<VFS_FileSystemQuery> m_fsQuery;

    // If true, we have 'm_startingDirectory'.  Otherwise, we are still
    // connecting, or processing the query to get that directory.
    bool m_haveStartingDirectory;

    // Directory to start in when switching to this connection in the
    // file browser.  This is populated by issuing a preliminary request
    // for the directory of "." as interpreted by the server.
    //
    // In this path, only forward slashes are used as directory
    // separators.
    std::string m_startingDirectory;

    // ID of the request currently being serviced by 'm_fsQuery', or 0
    // if none is.
    RequestID m_currentRequestID;

    // Sequence of queued requests to send.  VFS_FileSystemQuery only
    // allows one request at a time, so while a request is in flight,
    // additional requests are queued here.
    std::list<QueuedRequest> m_queuedRequests;

  public:      // methods
    // Establish a new connection to `hostName`, as a part of the
    // collection `connections`.
    //
    // Requires: connections != nullptr
    Connection(VFS_Connections *connections, HostName const &hostName);

    ~Connection();

    // Assert invariants.
    void selfCheck() const;

    // Report this connection's state.
    ConnectionState connectionState() const;

    // Enqueue 'req' as 'requestID', and send it when ready.
    void issueRequest(RequestID requestID, std::unique_ptr<VFS_Message> req);

    // True if 'requestID' has not received its reply.
    bool requestIsOutstanding(RequestID requestID) const;

    // If 'm_fsQuery' is ready, and there are queued requests, send the
    // next one.
    void issueQueuedRequest();

    // Cancel issuing and/or delivering reply for 'requestID'.  Return
    // true if the ID was found and canceled.
    bool cancelRequest(RequestID requestID);

    // Number of pending requests associated with this connection.
    int numOutstandingRequests() const;
  };

private:     // data
  // Map from a name to its connection info.
  //
  // The extrinsic order is that in which the hosts were passed to
  // `connect`.
  //
  // Invariant: For every key `k`:
  //   m_connections[k]->m_hostName == k
  smbase::OrderedMap<HostName, std::unique_ptr<Connection>>
    m_connections;

private:      // methods
  // Get the connection for the given hostName, or NULL if it is
  // invalid.
  //
  // Requires: isValid(hostName)
  Connection const * NULLABLE ifConnC(HostName const &hostName) const;
  Connection       * NULLABLE ifConn (HostName const &hostName)
    { return const_cast<Connection*>(ifConnC(hostName)); }

  // Get the connection for the given hostName.
  //
  // Requires: isValid(hostName)
  Connection const *connC(HostName const &hostName) const;
  Connection       *conn (HostName const &hostName)
    { return const_cast<Connection*>(connC(hostName)); }

  // Assuming we received a signal from a VFS_FileSystemQuery, find and
  // return the Connection associated with the sender.  Return nullptr
  // if one cannot be found.
  Connection * NULLABLE signalRecipientConnection();

public:      // methods
  VFS_Connections();
  virtual ~VFS_Connections() override;

  // Throw an exception if an invariant does not hold.
  void selfCheck() const override;

  // --------------------------- Connections ---------------------------
  // True if `connect(hostName)` has been called and
  // `shutdown(hostName)` has not.
  virtual ConnectionState connectionState(
    HostName const &hostName) const override;

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

  // Return the absolute path that clients should normally start in when
  // browsing the files at 'hostName'.
  //
  // Requires: isOrWasConnected(hostName)
  std::string getStartingDirectory(HostName const &hostName) const;

  // Shut down connection to 'hostName' and remove it from the set of
  // valid hosts.
  //
  // Requires: isValid(hostName)
  //
  // Ensures: !isValid(hostName)
  void shutdown(HostName const &hostName);

  // Shut down all connections.
  void shutdownAll();

  // ---------------------- Requests and replies -----------------------
  // `VFS_AbstractConnections` request methods.
  virtual void issueRequest(
    RequestID &requestID /*OUT*/,
    HostName const &hostName,
    std::unique_ptr<VFS_Message> req) override;
  virtual bool requestIsOutstanding(RequestID requestID) const override;
  virtual void cancelRequest(RequestID requestID) override;
  virtual int numOutstandingRequests() const override;

protected Q_SLOTS:
  // Handlers for VFS_FileSystemQuery.
  void on_vfsConnected() NOEXCEPT;
  void on_vfsReplyAvailable() NOEXCEPT;
  void on_vfsFailureAvailable() NOEXCEPT;
};


#endif // EDITOR_VFS_CONNECTIONS_H
