// vfs-query-sync.cc
// Code for vfs-query-sync.h.

#include "vfs-query-sync.h"            // this module

#include "waiting-counter.h"           // IncDecWaitingCounter

// smqtutil
#include "smqtutil/qstringb.h"         // qstringb
#include "smqtutil/qtguiutil.h"        // messageBox
#include "smqtutil/sync-wait.h"        // synchronouslyWaitUntil

// smbase
#include "smbase/trace.h"              // TRACE
#include "smbase/xassert.h"            // xassert

// qt
#include <QCursor>
#include <QGuiApplication>
#include <QProgressDialog>

// libc++
#include <utility>                     // std::move


VFS_QuerySync::VFS_QuerySync(
  VFS_Connections *vfsConnections,
  QWidget *parentWidget)
:
  m_vfsConnections(vfsConnections),
  m_parentWidget(parentWidget),
  m_requestID(0),
  m_hostName(HostName::asLocal()),
  m_reply(),
  m_connLostMessage()
{
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_vfsReplyAvailable,
                   this, &VFS_QuerySync::on_vfsReplyAvailable);
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_vfsFailed,
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
  m_vfsConnections->issueRequest(m_requestID,
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

  bool completed = synchronouslyWaitUntil(
    m_parentWidget,
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


void VFS_QuerySync::complain(string message)
{
  messageBox(m_parentWidget, "Error", message.c_str());
}


bool VFS_QuerySync::hfExists(HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_FileStatusRequest> req(new VFS_FileStatusRequest);
  req->m_path = harn.resourceName();

  std::unique_ptr<VFS_FileStatusReply> reply(
    issueTypedRequestSynchronously<VFS_FileStatusReply>(
      harn.hostName(), std::move(req)));

  if (reply) {
    return reply->m_success &&
           reply->m_fileKind == SMFileUtil::FK_REGULAR;
  }
  else {
    // If there is an error or the user cancels, we will say the file
    // does not exist.
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


std::unique_ptr<VFS_ReadFileReply> readFileSynchronously(
  VFS_Connections *vfsConnections,
  QWidget *parentWidget,
  HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_ReadFileRequest> req(new VFS_ReadFileRequest);
  req->m_path = harn.resourceName();

  VFS_QuerySync querySync(vfsConnections, parentWidget);
  return querySync.issueTypedRequestSynchronously<VFS_ReadFileReply>(
    harn.hostName(), std::move(req));
}


std::unique_ptr<VFS_FileStatusReply> getFileStatusSynchronously(
  VFS_Connections *vfsConnections,
  QWidget *parentWidget,
  HostAndResourceName const &harn)
{
  std::unique_ptr<VFS_FileStatusRequest> req(new VFS_FileStatusRequest);
  req->m_path = harn.resourceName();

  VFS_QuerySync querySync(vfsConnections, parentWidget);
  return querySync.issueTypedRequestSynchronously<VFS_FileStatusReply>(
    harn.hostName(), std::move(req));
}


// EOF
