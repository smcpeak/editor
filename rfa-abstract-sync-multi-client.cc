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


bool RFA_AbstractSyncMultiClient::hfExists(HostAndResourceName const &harn)
{
  smbase::UniquePtr<RFA_Msg_FileStatusRequest> req(new RFA_Msg_FileStatusRequest);
  req->m_path = harn.resourceName();

  auto replyOrError(
    issueTypedRequestSynchronously<RFA_Msg_FileStatusReply>(
      harn.hostName(), std::move(req)));

  if (replyOrError.isLeft()) {
    smbase::UniquePtr<RFA_Msg_FileStatusReply> &reply = replyOrError.left();
    if (reply) {
      return reply->m_success &&
             reply->m_fileKind == SMFileUtil::FK_REGULAR;
    }
    else {
      // If the user cancels, we will say the file does not exist.
      return false;
    }
  }
  else {
    TRACE1("hfExists(" << harn << "): " << replyOrError.right());

    // On error, just say the file does not exist.
    return false;
  }
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
