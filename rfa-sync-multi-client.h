// rfa-sync-multi-client.h
// RFA_SyncMultiClient class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_SYNC_MULTI_CLIENT_H
#define EDITOR_RFA_SYNC_MULTI_CLIENT_H

// editor
#include "fail-reason-opt.h"           // FailReasonOpt
#include "host-and-resource-name.h"    // HostAndResourceName
#include "host-name.h"                 // HostName
#include "ihfexists.h"                 // IHFExists
#include "reply-ptr-opt.h"             // ReplyPtrOpt, ReplyPtrOptOrError
#include "rfa-async-multi-client.h"    // RFA_AbstractAsyncMultiClient
#include "rfa-msg.h"                   // RFA_Message

// smqtutil
#include "smqtutil/sync-wait-fwd.h"    // SynchronousWaiter [f]

// Qt
#include <QObject>

// libc++
#include <type_traits>                 // std::is_base_of_v
#include <utility>                     // std::move


// Like RFA_AsyncMultiClient, but with a synchronous interface and an
// implementation that (via SynchronousWaiter) can be hooked up to a GUI
// to allow the user to cancel requests.
class RFA_SyncMultiClient : public QObject,
                            public IHFExists {
  Q_OBJECT
  NO_OBJECT_COPIES(RFA_SyncMultiClient);

public:      // types
  typedef RFA_AbstractAsyncMultiClient::RequestID RequestID;

private:     // data
  // Query interface to use.
  RFA_AbstractAsyncMultiClient *m_rfaAMClient;

  // Wait mechanism.
  SynchronousWaiter &m_waiter;

  // ID of outstanding request, or 0 if none.
  RequestID m_requestID;

  // If 'm_requestID' is not zero, this is the host being queried.
  // Otherwise, it is meaningless.
  HostName m_hostName;

  // Reply, if one has arrived.
  ReplyPtrOpt<RFA_Message> m_reply;

  // Connection lost message, or empty string.
  string m_connLostMessage;

public:      // instance methods
  // Create an object to issue queries via `rfaAMClient`.  Use
  // `waiter` to wait, which can (e.g.) pop up a modal window.
  RFA_SyncMultiClient(
    RFA_AbstractAsyncMultiClient *rfaAMClient, SynchronousWaiter &waiter);

  virtual ~RFA_SyncMultiClient() override;

  // Issue 'request' and wait for the reply.  If a reply arrives, set
  // 'reply' to it and return true.  If the connection is lost, set
  // 'connLostMessage' to the error message and return true.  If the
  // user cancels the request, return false.
  bool issueRequestSynchronously(
    HostName const &hostName,
    std::unique_ptr<RFA_Message> request,
    ReplyPtrOpt<RFA_Message> /*OUT*/ &reply,
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
  ReplyPtrOptOrError<REPLY_TYPE>
  issueTypedRequestSynchronously(
    HostName const &hostName,
    std::unique_ptr<RFA_Message> request);

  // Pop up a modal error dialog, returning when it is dismissed.
  void complain(string message);

  // IHFExists methods.
  virtual bool hfExists(HostAndResourceName const &harn) override;

protected Q_SLOTS:
  // Handlers for RFA_AbstractAsyncMultiClient.
  void on_rfaReplyAvailable(RequestID requestID) NOEXCEPT;
  void on_rfaFailed(HostName hostName, string reason) NOEXCEPT;
};


template <class REPLY_TYPE>
ReplyPtrOptOrError<REPLY_TYPE>
RFA_SyncMultiClient::issueTypedRequestSynchronously(
  HostName const &hostName,
  std::unique_ptr<RFA_Message> request)
{
  // Initially empty pointer.
  ReplyPtrOpt<REPLY_TYPE> typedReply;

  if (!m_rfaAMClient->isValid(hostName)) {
    return stringb("Host " << hostName << " is invalid.");
  }

  // Issue the request.
  ReplyPtrOpt<RFA_Message> genericReply;
  string connLostMessage;
  if (!issueRequestSynchronously(
         hostName, std::move(request), genericReply, connLostMessage)) {
    // Request was canceled; `typedReply` is still empty.
    return typedReply;
  }

  if (!connLostMessage.empty()) {
    return stringb("RFA connection lost: " << connLostMessage);
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
FailReasonOpt getROEErrorMessage(
  ReplyPtrOptOrError<REPLY_TYPE> const &replyOrError)
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


// Read the contents of `fname`, waiting for the reply and blocking user
// input during the wait (depending on what `waiter` does).
ReplyPtrOptOrError<RFA_Msg_ReadFileReply>
readFileSynchronously(
  RFA_AbstractAsyncMultiClient *rfaAMClient,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn);


// Get timestamp, etc., for 'fname'.
ReplyPtrOptOrError<RFA_Msg_FileStatusReply>
getFileStatusSynchronously(
  RFA_AbstractAsyncMultiClient *rfaAMClient,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn);


#endif // EDITOR_RFA_SYNC_MULTI_CLIENT_H
