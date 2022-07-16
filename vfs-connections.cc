// vfs-connections.cc
// Code for vfs-connections.h.

#include "vfs-connections.h"           // this module

// editor
#include "vfs-query.h"                 // FileSystemQuery

// smbase
#include "container-utils.h"           // contains
#include "map-utils.h"                 // insertMapUnique
#include "trace.h"                     // TRACE

// libc++
#include <utility>                     // std::move


VFS_Connections::VFS_Connections()
  : QObject(),
    m_fsQuery(new FileSystemQuery),
    m_nextRequestID(1),
    m_queuedRequests(),
    m_currentRequestID(0),
    m_pendingReplies()
{
  QObject::connect(m_fsQuery.get(), &FileSystemQuery::signal_connected,
                   this, &VFS_Connections::on_connected);
  QObject::connect(m_fsQuery.get(), &FileSystemQuery::signal_replyAvailable,
                   this, &VFS_Connections::on_replyAvailable);
  QObject::connect(m_fsQuery.get(), &FileSystemQuery::signal_failureAvailable,
                   this, &VFS_Connections::on_failureAvailable);
}


VFS_Connections::~VFS_Connections()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_fsQuery.get(), nullptr, this, nullptr);
}


void VFS_Connections::connectLocal()
{
  TRACE("VFS_Connections", "connectLocal");

  m_fsQuery->connectLocal();
}


bool VFS_Connections::localIsConnecting() const
{
  return m_fsQuery->isConnecting();
}


bool VFS_Connections::localIsReady() const
{
  return m_fsQuery->isReady();
}


void VFS_Connections::issueRequest(RequestID /*OUT*/ &requestID,
                                   std::unique_ptr<VFS_Message> req)
{
  requestID = m_nextRequestID++;
  m_queuedRequests.push_front(QueuedRequest(requestID, std::move(req)));
  TRACE("VFS_Connections", "enqueued request " << requestID);

  issuePendingRequest();
}


bool VFS_Connections::requestIsPending(RequestID requestID) const
{
  if (m_currentRequestID == requestID) {
    return true;
  }

  for (QueuedRequest const &qr : m_queuedRequests) {
    if (qr.m_requestID == requestID) {
      return true;
    }
  }

  return false;
}


bool VFS_Connections::replyIsAvailable(RequestID requestID) const
{
  return contains(m_pendingReplies, requestID);
}


void VFS_Connections::issuePendingRequest()
{
  if (!m_queuedRequests.empty()) {
    if (m_fsQuery->isReady()) {
      xassert(m_currentRequestID == 0);

      QueuedRequest const &qr = m_queuedRequests.back();
      m_currentRequestID = qr.m_requestID;
      m_fsQuery->sendRequest(*(qr.m_requestObject));
      m_queuedRequests.pop_back();

      TRACE("VFS_Connections", "sent request " << m_currentRequestID);
    }
    else {
      TRACE("VFS_Connections", "issuePendingRequest: connection not ready");
    }
  }
  else {
    TRACE("VFS_Connections", "issuePendingRequest: no queued requests");
  }
}


std::unique_ptr<VFS_Message> VFS_Connections::takeReply(
  RequestID requestID)
{
  TRACE("VFS_Connections", "takeReply(" << requestID << ")");

  auto it = m_pendingReplies.find(requestID);
  xassert(it != m_pendingReplies.end());

  std::unique_ptr<VFS_Message> ret(std::move((*it).second));

  m_pendingReplies.erase(it);

  return ret;
}


void VFS_Connections::cancelRequest(RequestID requestID)
{
  TRACE("VFS_Connections", "cancelRequest(" << requestID << ")");

  if (m_currentRequestID == requestID) {
    // This will signal for 'on_replyAvailable' to discard it.
    m_currentRequestID = 0;
    return;
  }

  // Remove a pending reply.
  auto it = m_pendingReplies.find(requestID);
  if (it != m_pendingReplies.end()) {
    m_pendingReplies.erase(it);
    return;
  }

  // Remove a queued request.  This is checked last because it could be
  // slow.
  for (auto it = m_queuedRequests.begin();
       it != m_queuedRequests.end(); ++it) {
    if ((*it).m_requestID == requestID) {
      m_queuedRequests.erase(it);
      return;
    }
  }
}


void VFS_Connections::shutdown()
{
  TRACE("VFS_Connections", "shutdown");

  m_fsQuery->shutdown();
}


bool VFS_Connections::connectionWasLost() const
{
  return m_fsQuery->hasFailed();
}


void VFS_Connections::on_connected()
{
  TRACE("VFS_Connections", "on_connected");

  issuePendingRequest();

  Q_EMIT signal_connected();
}


void VFS_Connections::on_replyAvailable()
{
  TRACE("VFS_Connections", "on_replyAvailable");

  if (m_currentRequestID == 0) {
    // The request was cancelled while in flight.
    m_fsQuery->takeReply();
  }
  else {
    // Get the ID so we can broadcast it.
    RequestID requestID = m_currentRequestID;

    // Save the reply for the client who presents the right ID.
    insertMapUniqueMove(m_pendingReplies,
      requestID, m_fsQuery->takeReply());

    // Clear the ID member so we know no request is outstanding.
    m_currentRequestID = 0;

    // Notify clients.
    Q_EMIT signal_replyAvailable(requestID);
  }

  issuePendingRequest();
}


void VFS_Connections::on_failureAvailable()
{
  TRACE("VFS_Connections", "on_failureAvailable");

  string reason = m_fsQuery->getFailureReason();
  m_currentRequestID = 0;

  Q_EMIT signal_vfsConnectionLost(reason);
}


// EOF
