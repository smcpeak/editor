// vfs-query-sync.cc
// Code for vfs-query-sync.h.

#include "vfs-query-sync.h"            // this module

// smbase
#include "trace.h"                     // TRACE
#include "xassert.h"                   // xassert

// qt
#include <QCursor>
#include <QGuiApplication>

// libc++
#include <utility>                     // std::move


VFS_QuerySync::VFS_QuerySync(
  VFS_Connections *vfsConnections,
  QWidget *parentWidget)
:
  m_vfsConnections(vfsConnections),
  m_parentWidget(parentWidget),
  m_requestID(0)
{
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_replyAvailable,
                   this, &VFS_QuerySync::on_replyAvailable);
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_vfsConnectionLost,
                   this, &VFS_QuerySync::on_vfsConnectionLost);
}


VFS_QuerySync::~VFS_QuerySync()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_vfsConnections, nullptr, this, nullptr);
}


bool VFS_QuerySync::issueRequestSynchronously(
  std::unique_ptr<VFS_Message> request,
  std::unique_ptr<VFS_Message> /*OUT*/ &reply,
  string /*OUT*/ &connLostMessage)
{
  xassert(!m_requestID);

  m_vfsConnections->issueRequest(m_requestID, std::move(request));
  RequestID origRequestID = m_requestID;

  TRACE("VFS_QuerySync",
    "request " << origRequestID << ": issued");

  QGuiApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

  while (m_requestID != 0) {
    // Pump the event queue but defer processing input events.
    TRACE("VFS_QuerySync",
      "request " << origRequestID << ": waiting for reply");
    m_eventLoop.exec(QEventLoop::ExcludeUserInputEvents);
  }

  QGuiApplication::restoreOverrideCursor();

  if (m_reply) {
    TRACE("VFS_QuerySync",
      "request " << origRequestID << ": got reply");
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


void VFS_QuerySync::on_replyAvailable(RequestID requestID) NOEXCEPT
{
  if (requestID == m_requestID) {
    m_reply = m_vfsConnections->takeReply(requestID);
    m_requestID = 0;
    m_eventLoop.exit();
  }
}


void VFS_QuerySync::on_vfsConnectionLost(string reason) NOEXCEPT
{
  if (m_requestID != 0) {
    m_connLostMessage = reason;
    m_requestID = 0;
    m_eventLoop.exit();
  }
}


// EOF
