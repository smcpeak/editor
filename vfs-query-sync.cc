// vfs-query-sync.cc
// Code for vfs-query-sync.h.

#include "vfs-query-sync.h"            // this module

// smqtutil
#include "smqtutil/qtguiutil.h"        // messageBox
#include "smqtutil/qtutil.h"           // qstringb

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
  m_connLostMessage(),
  m_eventLoop(),
  m_timer()
{
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_replyAvailable,
                   this, &VFS_QuerySync::on_vfsReplyAvailable);
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_failed,
                   this, &VFS_QuerySync::on_failed);
  QObject::connect(&m_timer, &QTimer::timeout,
                   this, &VFS_QuerySync::on_timeout);

  m_timer.setSingleShot(true);
}


VFS_QuerySync::~VFS_QuerySync()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_vfsConnections, nullptr, this, nullptr);
  QObject::disconnect(&m_timer, nullptr, this, nullptr);
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

  // Phase 1: No dialog.
  {
    // During the initial wait, we do not accept any input.
    QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

    m_timer.start(500 /*msec*/);

    while (m_requestID != 0 && m_timer.isActive()) {
      // Pump the event queue but defer processing input events.
      TRACE("VFS_QuerySync",
        "request " << origRequestID << ": waiting for reply, timer active");
      m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
    }

    m_timer.stop();

    QGuiApplication::restoreOverrideCursor();
  }

  // Phase 2: Progress dialog.
  if (m_requestID != 0) {
    // The "busy" cursor is a pointer with an hourglass or similar, to
    // indicate we are doing something, but will accept input.
    QGuiApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

    QProgressDialog progress(
      toQString(requestDescription),   // labelText
      "Cancel",                        // cancelButtonText,
      0, 1,                            // minimum and maximum,
      m_parentWidget);                 // parent
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0 /*ms*/);
    progress.setWindowTitle("Waiting for VFS query");

    QObject::connect(&progress, &QProgressDialog::canceled,
                     this, &VFS_QuerySync::on_canceled);

    // The minimum, maximum, and value do not make sense in this
    // situation, but I don't think I can just remove them.
    progress.setValue(0);

    while (m_requestID != 0 && !progress.wasCanceled()) {
      // Pump the event queue but defer processing input events.
      TRACE("VFS_QuerySync",
        "request " << origRequestID << ": waiting for reply, dialog active");
      m_eventLoop.exec();
    }

    // Dismiss the progress dialog.
    progress.reset();

    QGuiApplication::restoreOverrideCursor();
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
    TRACE("VFS_QuerySync",
      "request " << origRequestID << ": canceled");
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
    m_eventLoop.exit();
  }
}


void VFS_QuerySync::on_failed(
  HostName hostName, string reason) NOEXCEPT
{
  if (m_requestID != 0 && hostName == m_hostName) {
    m_connLostMessage = reason;
    m_requestID = 0;
    m_eventLoop.exit();
  }
}


void VFS_QuerySync::on_timeout() NOEXCEPT
{
  m_eventLoop.exit();
}


void VFS_QuerySync::on_canceled() NOEXCEPT
{
  m_eventLoop.exit();
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
