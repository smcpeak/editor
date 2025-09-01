// vfs-query-sync.cc
// Code for vfs-query-sync.h.

#include "vfs-query-sync.h"            // this module

#include "waiting-counter.h"           // IncDecWaitingCounter

// smqtutil
#include "smqtutil/qstringb.h"         // qstringb
#include "smqtutil/qtguiutil.h"        // messageBox
#include "smqtutil/sync-wait.h"        // synchronouslyWaitUntil

// smbase
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/trace.h"              // TRACE
#include "smbase/xassert.h"            // xassert

// qt
#include <QCursor>
#include <QGuiApplication>
#include <QProgressDialog>

// libc++
#include <utility>                     // std::move


VFS_QuerySync::VFS_QuerySync(
  VFS_AbstractConnections *vfsConnections, SynchronousWaiter &waiter)
  : IMEMBFP(vfsConnections),
    IMEMBFP(waiter),
    m_requestID(0),
    m_hostName(HostName::asLocal()),
    m_reply(),
    m_connLostMessage()
{
  QObject::connect(
    m_vfsConnections, &VFS_AbstractConnections::signal_vfsReplyAvailable,
                              this, &VFS_QuerySync::on_vfsReplyAvailable);

  QObject::connect(
    m_vfsConnections, &VFS_AbstractConnections::signal_vfsFailed,
                              this, &VFS_QuerySync::on_vfsFailed);
}


VFS_QuerySync::~VFS_QuerySync()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_vfsConnections, nullptr, this, nullptr);
}


bool VFS_QuerySync::issueRequestSynchronously(
  HostName const &hostName,
  std::unique_ptr<VFS_Message> request,
  std::unique_ptr<VFS_Message> /*OUT*/ &reply,
  string /*OUT*/ &connLostMessage)
{
  xassert(!m_requestID);
  m_hostName = hostName;

  string requestDescription = request->description();
  m_vfsConnections->issueRequest(m_requestID /*OUT*/,
    m_hostName, std::move(request));
  RequestID origRequestID = m_requestID;

  TRACE("VFS_QuerySync",
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
    "Waiting for VFS query",
    requestDescription);

  if (!completed) {
    TRACE("VFS_QuerySync",
      "request " << origRequestID << ": canceled");
    m_requestID = 0;
    return false;
  }

  if (m_reply) {
    TRACE("VFS_QuerySync",
      "request " << origRequestID <<
      ": got reply: " << m_reply->description());
    reply = std::move(m_reply);
    return true;
  }
  else if (!m_connLostMessage.empty()) {
    TRACE("VFS_QuerySync",
      "request " << origRequestID <<
      ": conn lost: " << m_connLostMessage);
    connLostMessage = m_connLostMessage;
    m_connLostMessage = "";
    return true;
  }
  else {
    // Should not happen.
    TRACE("VFS_QuerySync",
      "request " << origRequestID << ": what happened?");
    DEV_WARNING("VFS_QuerySync: not canceled, succeeded, nor failed?");
    return false;
  }
}


bool VFS_QuerySync::hfExists(HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_FileStatusRequest> req(new VFS_FileStatusRequest);
  req->m_path = harn.resourceName();

  auto replyOrError(
    issueTypedRequestSynchronously<VFS_FileStatusReply>(
      harn.hostName(), std::move(req)));

  if (replyOrError.isLeft()) {
    std::unique_ptr<VFS_FileStatusReply> &reply = replyOrError.left();
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
    TRACE("VFS_QuerySync",
      "hfExists(" << harn << "): " << replyOrError.right());

    // On error, just say the file does not exist.
    return false;
  }
}


void VFS_QuerySync::on_vfsReplyAvailable(RequestID requestID) NOEXCEPT
{
  if (requestID == m_requestID) {
    m_reply = m_vfsConnections->takeReply(requestID);
    m_requestID = 0;
  }
}


void VFS_QuerySync::on_vfsFailed(
  HostName hostName, string reason) NOEXCEPT
{
  if (m_requestID != 0 && hostName == m_hostName) {
    m_connLostMessage = reason;
    m_requestID = 0;
  }
}


smbase::Either<std::unique_ptr<VFS_ReadFileReply>, std::string>
readFileSynchronously(
  VFS_AbstractConnections *vfsConnections,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_ReadFileRequest> req(new VFS_ReadFileRequest);
  req->m_path = harn.resourceName();

  VFS_QuerySync querySync(vfsConnections, waiter);
  return querySync.issueTypedRequestSynchronously<VFS_ReadFileReply>(
    harn.hostName(), std::move(req));
}


smbase::Either<std::unique_ptr<VFS_FileStatusReply>, std::string>
getFileStatusSynchronously(
  VFS_AbstractConnections *vfsConnections,
  SynchronousWaiter &waiter,
  HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_FileStatusRequest> req(new VFS_FileStatusRequest);
  req->m_path = harn.resourceName();

  VFS_QuerySync querySync(vfsConnections, waiter);
  return querySync.issueTypedRequestSynchronously<VFS_FileStatusReply>(
    harn.hostName(), std::move(req));
}


// EOF
