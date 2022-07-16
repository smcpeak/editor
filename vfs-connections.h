// vfs-connections.h
// VFS_Connections class.

#ifndef EDITOR_VFS_CONNECTIONS_H
#define EDITOR_VFS_CONNECTIONS_H

#include "vfs-connections-fwd.h"       // fwds for this module

// editor
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


// Collection of active FileSystemQuery objects, and an asynchronous
// query interface on top of them.
class VFS_Connections : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_Connections);

public:      // types
  // Unique identifier for a request.  Never zero for a valid request.
  typedef uint64_t RequestID;

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

private:     // data
  // For now, there is only one query object.
  std::unique_ptr<FileSystemQuery> m_fsQuery;

  // Next ID to assign to a request.
  RequestID m_nextRequestID;

  // Sequence of queued requests to send.
  std::list<QueuedRequest> m_queuedRequests;

  // ID of the request currently being serviced by 'm_fsQuery'.
  RequestID m_currentRequestID;

  // Map from request ID to the corresponding reply object, for those
  // requests whose reply is available.
  std::map<RequestID, std::unique_ptr<VFS_Message> > m_pendingReplies;

private:      // methods
  // If 'm_fsQuery' is ready, and there are queued requests, send the
  // next one.
  void issuePendingRequest();

public:      // methods
  VFS_Connections();
  virtual ~VFS_Connections() override;

  // Establish the connection for local file system access.
  void connectLocal();

  // True while we are setting up the local connection.
  bool localIsConnecting() const;

  // True when the local connection is ready to receive requests.  This
  // is true even if a request is being processed, so long we can
  // enqueue another.
  bool localIsReady() const;

  // Issue new request 'req'.  The request ID is stored in 'requestID'
  // *before* anything is done that might lead to delivery of the
  // reply.
  void issueRequest(RequestID /*OUT*/ &requestID,
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
  // has not already been delivered.  Remove it from 'm_pendingReplies'
  // if it is present there.
  void cancelRequest(RequestID requestID);

  // Shut down all connections.
  void shutdown();

  // True if the connection was lost due to a reason other than a call
  // to 'shutdown'.
  bool connectionWasLost() const;

Q_SIGNALS:
  // Emitted when 'isConnecting()' transitions to 'isReady()'.
  void signal_connected();

  // Emitted when 'replyIsAvailable(requestID)' becomes true.
  void signal_replyAvailable(RequestID requestID);

  // Emitted when 'connectionWasLost' becomes true.
  void signal_vfsConnectionLost(string reason);

protected Q_SLOTS:
  // Handlers for FileSystemQuery.
  void on_connected();
  void on_replyAvailable();
  void on_failureAvailable();
};


#endif // EDITOR_VFS_CONNECTIONS_H
