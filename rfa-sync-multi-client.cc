// rfa-sync-multi-client.cc
// Code for rfa-sync-multi-client.h.

#include "rfa-sync-multi-client.h"     // this module

// editor
#include "waiting-counter.h"           // IncDecWaitingCounter

// smqtutil
#include "smqtutil/qstringb.h"         // qstringb
#include "smqtutil/qtguiutil.h"        // messageBox
#include "smqtutil/sync-wait.h"        // synchronouslyWaitUntil

// smbase
#include "smbase/htt.h"                // HTT_TASK
#include "smbase/sm-macros.h"          // IMEMBFP, USED_FOR_DEBUGGING
#include "smbase/trace.h"              // TRACE
#include "smbase/xassert.h"            // xassert

// libc++
#include <utility>                     // std::move


RFA_SyncMultiClient::RFA_SyncMultiClient(
  RFA_AbstractAsyncMultiClient *rfaAMClient, SynchronousWaiter &waiter)
  : IMEMBFP(rfaAMClient),
    IMEMBFP(waiter),
    m_requestID(0),
    m_hostName(HostName::asLocal()),
    m_reply(),
    m_connLostMessage()
{
  QObject::connect(
    m_rfaAMClient, &RFA_AbstractAsyncMultiClient::signal_rfaReplyAvailable,
    this,          &RFA_SyncMultiClient::             on_rfaReplyAvailable);

  QObject::connect(
    m_rfaAMClient, &RFA_AbstractAsyncMultiClient::signal_rfaFailed,
    this,          &RFA_SyncMultiClient::             on_rfaFailed);
}


RFA_SyncMultiClient::~RFA_SyncMultiClient()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_rfaAMClient, nullptr, this, nullptr);
}


bool RFA_SyncMultiClient::issueRequestSynchronously(
  HostName const &hostName,
  std::unique_ptr<RFA_Message> request,
  std::unique_ptr<RFA_Message> /*OUT*/ &reply,
  string /*OUT*/ &connLostMessage)
{
  xassert(!m_requestID);
  m_hostName = hostName;

  string requestDescription = request->description();
  m_rfaAMClient->issueRequest(m_requestID /*OUT*/,
    m_hostName, std::move(request));

  USED_FOR_DEBUGGING RequestID origRequestID = m_requestID;

  TRACE("RFA_SyncMultiClient",
    "request " << origRequestID << ": issued: " << requestDescription);

  // Inform the test infrastructure that we are awaiting IPC.
  IncDecWaitingCounter idwc;

  auto doneCondition = [this]() -> bool {
    // When the correct reply arrives, or a failure happens, we reset
    // this to zero.
    return this->m_requestID == 0;
  };

  bool completed = m_waiter.waitUntil(
    doneCondition,
    500 /*msec*/,
    "Waiting for RFA query",
    requestDescription);

  if (!completed) {
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID << ": canceled");
    m_requestID = 0;
    return false;
  }

  if (m_reply) {
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID <<
      ": got reply: " << m_reply->description());
    reply = std::move(m_reply);
    return true;
  }
  else if (!m_connLostMessage.empty()) {
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID <<
      ": conn lost: " << m_connLostMessage);
    connLostMessage = m_connLostMessage;
    m_connLostMessage = "";
    return true;
  }
  else {
    // Should not happen.
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID << ": what happened?");
    DEV_WARNING("RFA_SyncMultiClient: not canceled, succeeded, nor failed?");
    return false;
  }
}


bool RFA_SyncMultiClient::hfExists(HostAndResourceName const &harn)
{
  std::unique_ptr<RFA_Msg_FileStatusRequest> req(new RFA_Msg_FileStatusRequest);
  req->m_path = harn.resourceName();

  auto replyOrError(
    issueTypedRequestSynchronously<RFA_Msg_FileStatusReply>(
      harn.hostName(), std::move(req)));

  if (replyOrError.isLeft()) {
    std::unique_ptr<RFA_Msg_FileStatusReply> &reply = replyOrError.left();
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
    TRACE("RFA_SyncMultiClient",
      "hfExists(" << harn << "): " << replyOrError.right());

    // On error, just say the file does not exist.
    return false;
  }
}


void RFA_SyncMultiClient::on_rfaReplyAvailable(RequestID requestID) NOEXCEPT
{
  if (requestID == m_requestID) {
    m_reply = m_rfaAMClient->takeReply(requestID);
    m_requestID = 0;
  }
}


void RFA_SyncMultiClient::on_rfaFailed(
  HostName hostName, string reason) NOEXCEPT
{
  if (m_requestID != 0 && hostName == m_hostName) {
    m_connLostMessage = reason;
    m_requestID = 0;
  }
}


smbase::Either<std::unique_ptr<RFA_Msg_ReadFileReply>, std::string>
readFileSynchronously(
  RFA_AbstractAsyncMultiClient *rfaAMClient,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn)
{
  HTT_TASK("readFileSynchronously");

  std::unique_ptr<RFA_Msg_ReadFileRequest> req(new RFA_Msg_ReadFileRequest);
  req->m_path = harn.resourceName();

  RFA_SyncMultiClient rfaSMClient(rfaAMClient, waiter);
  return rfaSMClient.issueTypedRequestSynchronously<RFA_Msg_ReadFileReply>(
    harn.hostName(), std::move(req));
}


smbase::Either<std::unique_ptr<RFA_Msg_FileStatusReply>, std::string>
getFileStatusSynchronously(
  RFA_AbstractAsyncMultiClient *rfaAMClient,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn)
{
  std::unique_ptr<RFA_Msg_FileStatusRequest> req(new RFA_Msg_FileStatusRequest);
  req->m_path = harn.resourceName();

  RFA_SyncMultiClient rfaSMClient(rfaAMClient, waiter);
  return rfaSMClient.issueTypedRequestSynchronously<RFA_Msg_FileStatusReply>(
    harn.hostName(), std::move(req));
}


// EOF
