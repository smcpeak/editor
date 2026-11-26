// rfa-sync-multi-client.cc
// Code for rfa-sync-multi-client.h.

#include "rfa-sync-multi-client.h"               // this module

// editor
#include "waiting-counter.h"                     // IncDecWaitingCounter

// smqtutil
#include "smqtutil/qstringb.h"                   // qstringb
#include "smqtutil/qtguiutil.h"                  // messageBox
#include "smqtutil/qtutil.h"                     // disconnectSignals
#include "smqtutil/sync-wait.h"                  // SynchronousWaiter

// smbase
#include "smbase/htt.h"                          // HTT_TASK
#include "smbase/sm-macros.h"                    // IMEMBFP, USED_FOR_DEBUGGING
#include "smbase/sm-unique-ptr-std-conv.h"       // smbase::UniquePtr <-> std::unique_ptr
#include "smbase/trace.h"                        // TRACE
#include "smbase/xassert.h"                      // xassert

// libc++
#include <utility>                               // std::move


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
  disconnectSignals(m_rfaAMClient, this);
}


ReplyPtrOptOrError<RFA_Message>
RFA_SyncMultiClient::issueRequestSynchronously(
  HostName const &hostName,
  smbase::UniquePtr<RFA_Message> request)
{
  xassert(!m_requestID);

  if (!m_rfaAMClient->isValid(hostName)) {
    return ReplyPtrOptOrError<RFA_Message>(
      stringb("Host " << hostName << " is invalid."));
  }

  m_hostName = hostName;

  string requestDescription = request->description();
  m_rfaAMClient->issueRequest(m_requestID /*OUT*/,
    m_hostName, std::move(request));

  USED_FOR_DEBUGGING
  RequestID origRequestID = m_requestID;

  TRACE("RFA_SyncMultiClient",
    "request " << origRequestID << ": issued: " << requestDescription);

  // Inform the UI shell infrastructure that we are awaiting IPC.
  IncDecWaitingCounter idwc;

  auto doneCondition = [this]() -> bool {
    // When the correct reply arrives, or a failure happens, we reset
    // this to zero.
    return this->m_requestID == 0;
  };

  bool completed = m_waiter.waitUntil(
    doneCondition,
    500 /*msec*/,
    "Waiting for Remote File Access reply",
    requestDescription);

  if (!completed) {
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID << ": canceled");
    m_requestID = 0;
    return ReplyPtrOptOrError<RFA_Message>();
  }

  if (m_reply) {
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID <<
      ": got reply: " << m_reply->description());
    return ReplyPtrOptOrError<RFA_Message>(std::move(m_reply));
  }
  else if (!m_connLostMessage.empty()) {
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID <<
      ": conn lost: " << m_connLostMessage);
    ReplyPtrOptOrError<RFA_Message> ret(m_connLostMessage);
    m_connLostMessage = "";
    return ret;
  }
  else {
    // Should not happen.
    TRACE("RFA_SyncMultiClient",
      "request " << origRequestID << ": what happened?");
    DEV_WARNING("RFA_SyncMultiClient: not canceled, succeeded, nor failed?");
    return ReplyPtrOptOrError<RFA_Message>();
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


// EOF
