// vfs-query-sync.h
// VFS_QuerySync class.

#ifndef EDITOR_VFS_QUERY_SYNC_H
#define EDITOR_VFS_QUERY_SYNC_H

#include "host-and-resource-name.h"    // HostAndResourceName
#include "host-name.h"                 // HostName
#include "nearby-file.h"               // IHFExists
#include "vfs-connections.h"           // VFS_Connections

#include "smqtutil/sync-wait.h"        // SynchronousWaiter

#include "smbase/either.h"             // smbase::Either

#include <QObject>

#include <optional>                    // std::optional
#include <string>                      // std::string


// Like VFS_FileSystemQuery, but with a synchronous interface and an
// implementation that has a GUI to allow the user to cancel requests.
class VFS_QuerySync : public QObject,
                      public IHFExists {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_QuerySync);

public:      // types
  typedef VFS_Connections::RequestID RequestID;

private:     // data
  // Query interface to use.
  VFS_Connections *m_vfsConnections;

  // Wait mechanism.
  SynchronousWaiter &m_waiter;

  // ID of outstanding request, or 0 if none.
  RequestID m_requestID;

  // If 'm_requestID' is not zero, this is the host being queried.
  // Otherwise, it is meaningless.
  HostName m_hostName;

  // Reply, if one has arrived.
  std::unique_ptr<VFS_Message> m_reply;

  // Connection lost message, or empty string.
  string m_connLostMessage;

public:      // instance methods
  // Create an object to issue queries via `vfsConnections`.  Use
  // `waiter` to wait, which can (e.g.) pop up a modal window.
  VFS_QuerySync(
    VFS_Connections *vfsConnections, SynchronousWaiter &waiter);

  virtual ~VFS_QuerySync() override;

  // Issue 'request' and wait for the reply.  If a reply arrives, set
  // 'reply' to it and return true.  If the connection is lost, set
  // 'connLostMessage' to the error message and return true.  If the
  // user cancels the request, return false.
  bool issueRequestSynchronously(
    HostName const &hostName,
    std::unique_ptr<VFS_Message> request,
    std::unique_ptr<VFS_Message> /*OUT*/ &reply,
    string /*OUT*/ &connLostMessage);

  // Issue 'request' synchronously, expecting to get 'REPLY_TYPE' in the
  // left alternative.  Note that it could be a failure reply.
  //
  // If there is an error, including if 'hostName' is invalid, return an
  // error message in the right alternative.
  //
  // If the request is canceled, return the left alternative with a null
  // pointer.
  //
  template <class REPLY_TYPE>
  smbase::Either<std::unique_ptr<REPLY_TYPE>, std::string>
  issueTypedRequestSynchronously(
    HostName const &hostName,
    std::unique_ptr<VFS_Message> request);

  // Pop up a modal error dialog, returning when it is dismissed.
  void complain(string message);

  // IHFExists methods.
  virtual bool hfExists(HostAndResourceName const &harn) override;

protected Q_SLOTS:
  // Handlers for VFS_Connections.
  void on_vfsReplyAvailable(RequestID requestID) NOEXCEPT;
  void on_vfsFailed(HostName hostName, string reason) NOEXCEPT;
};


template <class REPLY_TYPE>
smbase::Either<std::unique_ptr<REPLY_TYPE>, std::string>
VFS_QuerySync::issueTypedRequestSynchronously(
  HostName const &hostName,
  std::unique_ptr<VFS_Message> request)
{
  // Initially empty pointer, used for error returns.
  std::unique_ptr<REPLY_TYPE> typedReply;

  if (!m_vfsConnections->isValid(hostName)) {
    return stringb("Host " << hostName << " is invalid.");
  }

  // Issue the request.
  std::unique_ptr<VFS_Message> genericReply;
  string connLostMessage;
  if (!issueRequestSynchronously(
         hostName, std::move(request), genericReply, connLostMessage)) {
    // Request was canceled.
    return typedReply;
  }

  if (!connLostMessage.empty()) {
    return stringb("VFS connection lost: " << connLostMessage);
  }

  if (REPLY_TYPE *r = dynamic_cast<REPLY_TYPE*>(genericReply.get())) {
    // Move the pointer from `genericReply` to `typedReply`.  This
    // discards the return value from `release()` because it has the
    // generic type, while `r` has the specific type.
    (void)genericReply.release();
    typedReply.reset(r);
    return typedReply;
  }

  else {
    return stringb(
      "Server responded with incorrect message type: " <<
      toString(genericReply->messageType()));
  }
}


/* If `replyOrError` has an error message in either half, extract and
   return it.

   Ensures:
     if return == nullopt:
       replyOrError.isLeft() == true and
       if replyOrError.left() != nullptr:
         replyOrError.left()->m_success == true
*/
template <class REPLY_TYPE>
std::optional<std::string> getROEErrorMessage(
  smbase::Either<std::unique_ptr<REPLY_TYPE>, std::string>
    const &replyOrError)
{
  if (replyOrError.isRight()) {
    return replyOrError.right();
  }
  else {
    std::unique_ptr<REPLY_TYPE> const &reply = replyOrError.left();
    if (reply && !reply->m_success) {
      return stringb(
        reply->m_failureReasonString <<
        " (code " << reply->m_failureReasonCode << ")");
    }
  }

  return std::nullopt;
}


/* Read the contents of `fname`, waiting for the reply and blocking user
   input during the wait (depending on what `waiter` does).

   There are four return value cases:

     1. Success reply in left alternative: read the file.

     2. Error reply in left alternative: error during read attempt on
        the server, for example the file does not exist or there was a
        file system permission issue.

     3. Null pointer in left alternative: user canceled the wait.

     4. Error message in right alternative: VFS communication error, for
        example the connection was terminated.

   The `getROEErrorMessage` function above can be used to combine the
   handling of the error cases (2 and 4).
*/
smbase::Either<std::unique_ptr<VFS_ReadFileReply>, std::string>
readFileSynchronously(
  VFS_Connections *vfsConnections,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn);


// Get timestamp, etc., for 'fname'.
//
// This has the same return cases as `readFileSynchronously`.
smbase::Either<std::unique_ptr<VFS_FileStatusReply>, std::string>
getFileStatusSynchronously(
  VFS_Connections *vfsConnections,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn);


#endif // EDITOR_VFS_QUERY_SYNC_H
