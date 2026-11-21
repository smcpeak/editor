// rfa-abstract-sync-multi-client.h
// `RFA_AbstractSyncMultiClient` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_ABSTRACT_SYNC_MULTI_CLIENT_H
#define EDITOR_RFA_ABSTRACT_SYNC_MULTI_CLIENT_H

// editor
#include "host-and-resource-name-fwd.h"          // HostAndResourceName [f]
#include "host-name-fwd.h"                       // HostName [f]
#include "ihfexists.h"                           // IHFExists
#include "reply-ptr-opt.h"                       // ReplyPtrOptOrError
#include "rfa-msg.h"                             // RFA_Message, RFA_Msg_ReadFileReply

// smbase
#include "smbase/sm-unique-ptr.h"                // smbase::UniquePtr
#include "smbase/std-optional-fwd.h"             // std::optional [f]
#include "smbase/stringb.h"                      // stringb
#include "smbase/xassert.h"                      // xassert

// libc++
#include <utility>                               // std::move


/*
  This declares the `issueRequestSynchronously` interface and defines
  the typed wrapper on top of it.

  It also defines some methods that encapsulate the creation of RFA
  messages.  This interface thus provides a sort of distributed file
  system, albeit with resources explicitly tied to specific hosts.

  This module does not depend on anything in Qt.
*/
class RFA_AbstractSyncMultiClient : public IHFExists {
public:      // methods
  virtual ~RFA_AbstractSyncMultiClient();

  RFA_AbstractSyncMultiClient();

  // Issue 'request' and wait for the reply while pumping the event
  // queue.
  virtual ReplyPtrOptOrError<RFA_Message> issueRequestSynchronously(
    HostName const &hostName,
    smbase::UniquePtr<RFA_Message> request) = 0;

  // Issue 'request' synchronously, expecting to get 'REPLY_TYPE'.
  template <class REPLY_TYPE>
  ReplyPtrOptOrError<REPLY_TYPE> issueTypedRequestSynchronously(
    HostName const &hostName,
    smbase::UniquePtr<RFA_Message> request);

  // IHFExists methods.
  virtual std::optional<FileTimestamp> hfModTime(
    HostAndResourceName const &harn) override;

  // ---- Methods that encapsulate request creation ----
  // Read the contents of `harn`.
  ReplyPtrOptOrError<RFA_Msg_ReadFileReply>
  readFileSynchronously(
    HostAndResourceName const &harn);

  // Get timestamp, etc., for 'harn'.
  ReplyPtrOptOrError<RFA_Msg_FileStatusReply>
  getFileStatusSynchronously(
    HostAndResourceName const &harn);
};


template <class REPLY_TYPE>
ReplyPtrOptOrError<REPLY_TYPE>
RFA_AbstractSyncMultiClient::issueTypedRequestSynchronously(
  HostName const &hostName,
  smbase::UniquePtr<RFA_Message> request)
{
  // Issue the request using the generic interface.
  ReplyPtrOptOrError<RFA_Message> replyOrError =
    this->issueRequestSynchronously(hostName, std::move(request));

  if (replyOrError.isRight()) {
    // Relay the error message.
    return ReplyPtrOptOrError<REPLY_TYPE>(replyOrError.rightC());
  }

  xassert(replyOrError.isLeft());
  ReplyPtrOpt<RFA_Message> &reply = replyOrError.left();

  if (!reply.hasValue()) {
    // Relay the absent value.
    return ReplyPtrOptOrError<REPLY_TYPE>();
  }

  if (REPLY_TYPE *r = dynamic_cast<REPLY_TYPE*>(reply.get())) {
    // Move the pointer from `genericReply` to a `ReplyPtrOptOrError`
    // with a more specific message type.  This discards the return
    // value from `release()` because it has the generic type, while `r`
    // has the specific type.
    (void)reply.release();
    return ReplyPtrOptOrError<REPLY_TYPE>(r);
  }

  else {
    return ReplyPtrOptOrError<REPLY_TYPE>(stringb(
      "Server responded with incorrect message type: " <<
      toString(reply->messageType())));
  }
}


#endif // EDITOR_RFA_ABSTRACT_SYNC_MULTI_CLIENT_H
