// rfa-abstract-sync-multi-client.cc
// Code for `rfa-abstract-sync-multi-client` module.

#include "rfa-abstract-sync-multi-client.h"      // this module

// editor
#include "host-and-resource-name.h"              // HostAndResourceName
#include "rfa-msg.h"                             // RFA_Msg_FileStatus{Request, Reply}

// smbase
#include "smbase/htt.h"                          // HTT_TASK
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-trace.h"                     // INIT_TRACE, TRACE
#include "smbase/sm-unique-ptr.h"                // smbase::UniquePtr

// libc++
#include <utility>                               // std::move


INIT_TRACE("rfa-abstract-sync-multi-client");


RFA_AbstractSyncMultiClient::~RFA_AbstractSyncMultiClient()
{}


RFA_AbstractSyncMultiClient::RFA_AbstractSyncMultiClient()
{}


std::optional<FileTimestamp> RFA_AbstractSyncMultiClient::hfModTime(
  HostAndResourceName const &harn)
{
  smbase::UniquePtr<RFA_Msg_FileStatusRequest> req(new RFA_Msg_FileStatusRequest);
  req->m_path = harn.resourceName();

  auto replyOrError(
    issueTypedRequestSynchronously<RFA_Msg_FileStatusReply>(
      harn.hostName(), std::move(req)));

  if (FailReasonOpt reason = replyOrError.getEitherErrorMessage()) {
    TRACE1("hfModTime(" << harn << "): " << *reason);
    return std::nullopt;
  }

  xassert(replyOrError.isLeft());
  smbase::UniquePtr<RFA_Msg_FileStatusReply> &reply = replyOrError.left();
  if (reply) {
    xassert(reply->m_success);
    if (reply->m_fileKind == SMFileUtil::FK_REGULAR) {
      return reply->m_fileModificationTime;
    }
  }

  // User canceled or the file was a different kind.
  return std::nullopt;
}


ReplyPtrOptOrError<RFA_Msg_ReadFileReply>
RFA_AbstractSyncMultiClient::readFileSynchronously(
  HostAndResourceName const &harn)
{
  HTT_TASK("readFileSynchronously");

  smbase::UniquePtr<RFA_Msg_ReadFileRequest> req(new RFA_Msg_ReadFileRequest);
  req->m_path = harn.resourceName();

  return issueTypedRequestSynchronously<RFA_Msg_ReadFileReply>(
    harn.hostName(), std::move(req));
}


ReplyPtrOptOrError<RFA_Msg_FileStatusReply>
RFA_AbstractSyncMultiClient::getFileStatusSynchronously(
  HostAndResourceName const &harn)
{
  smbase::UniquePtr<RFA_Msg_FileStatusRequest> req(new RFA_Msg_FileStatusRequest);
  req->m_path = harn.resourceName();

  return issueTypedRequestSynchronously<RFA_Msg_FileStatusReply>(
    harn.hostName(), std::move(req));
}


// EOF
