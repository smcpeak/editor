// vfs-test-connections.cc
// Code for `vfs-test-connections` module.

#include "vfs-test-connections.h"                // this module

#include "smbase/either.h"                       // smbase::Either
#include "smbase/map-util.h"                     // smbase::{mapInsertUniqueMove, mapInsertUnique}
#include "smbase/overflow.h"                     // safeToInt
#include "smbase/portable-error-code.h"          // smbase::{PortableErrorCode, portableCodeDescription}
#include "smbase/set-util.h"                     // smbase::setIsDisjointWith
#include "smbase/sm-trace.h"                     // INIT_TRACE, etc.
#include "smbase/string-util.h"                  // stringToVectorOfUChar
#include "smbase/xassert.h"                      // xassert

#include <memory>                                // std::unique_ptr
#include <utility>                               // std::move

using namespace gdv;
using namespace smbase;


INIT_TRACE("vfs-test-connections");


// An explicit definition of this dtor is required because, without it,
// it gets implicitly defined in the .moc.cc file, which only has a
// forward declaration of `smbase::Either`, so that fails to compile.
VFS_TestConnections::~VFS_TestConnections()
{}


VFS_TestConnections::VFS_TestConnections()
  : m_hosts(),
    m_issuedRequests()
{
  // The purpose of this signal and slot pair is to defer processing
  // until we return to the main event loop.
  QObject::connect(
    this, &VFS_TestConnections::signal_processRequests,
    this, &VFS_TestConnections::  slot_processRequests,
    Qt::QueuedConnection);
}


void VFS_TestConnections::selfCheck() const
{
  VFS_AbstractConnections::selfCheck();

  // There should not be any overlap in the domains of
  // `m_issuedRequests` and `m_availableReplies`.
  xassert(setIsDisjointWith(
    mapKeySet(m_issuedRequests),
    mapKeySet(m_availableReplies)));
}


VFS_TestConnections::ConnectionState
VFS_TestConnections::connectionState(
  HostName const &hostName) const
{
  if (auto value = mapFindOpt(m_hosts, hostName)) {
    return (**value).second;
  }
  else {
    return CS_INVALID;
  }
}


void VFS_TestConnections::issueRequest(
  RequestID &requestID /*OUT*/,
  HostName const &hostName,
  std::unique_ptr<VFS_Message> req)
{
  requestID = m_nextRequestID++;
  mapInsertUniqueMove(m_issuedRequests, requestID, std::move(req));

  TRACE1("emitting signal_processRequests");
  Q_EMIT signal_processRequests();
}


bool VFS_TestConnections::requestIsOutstanding(RequestID requestID) const
{
  return mapContains(m_issuedRequests, requestID);
}


void VFS_TestConnections::cancelRequest(RequestID requestID)
{
  // I don't call this in this test.
  xfailure("unimplemented");
}


int VFS_TestConnections::numOutstandingRequests() const
{
  return safeToInt(m_issuedRequests.size());
}


void VFS_TestConnections::slot_processRequests()
{
  TRACE1("in slot_processRequests; num issued requests is " <<
         m_issuedRequests.size());

  while (!m_issuedRequests.empty()) {
    // Dequeue the next request.
    auto it = m_issuedRequests.begin();
    RequestID id = (*it).first;
    std::unique_ptr<VFS_Message> msg = std::move((*it).second);
    m_issuedRequests.erase(it);

    if (auto rfr = dynamic_cast<VFS_ReadFileRequest const *>(msg.get())) {
      mapInsertUniqueMove(m_availableReplies, id,
        std::unique_ptr<VFS_Message>(processRFR(rfr).release()));

      TRACE1("emitting signal_vfsReplyAvailable(" << id << ")");
      Q_EMIT signal_vfsReplyAvailable(id);
    }

    else {
      xfailure("unrecognized message");
    }
  }
}


std::unique_ptr<VFS_ReadFileReply> VFS_TestConnections::processRFR(
  VFS_ReadFileRequest const *rfr)
{
  std::unique_ptr<VFS_ReadFileReply> reply(new VFS_ReadFileReply);

  if (auto itOpt = mapFindOpt(m_files, rfr->m_path)) {
    FileReplyData const &fdata = (**itOpt).second;

    if (fdata.isLeft()) {
      reply->m_success = true;
      reply->m_contents = stringToVectorOfUChar(fdata.left());
    }
    else {
      PortableErrorCode code = fdata.right();
      reply->m_success = false;
      reply->m_failureReasonCode = code;
      reply->m_failureReasonString = portableCodeDescription(code);
    }
  }
  else {
    reply->m_success = false;
    reply->m_failureReasonCode = PortableErrorCode::PEC_FILE_NOT_FOUND;
    reply->m_failureReasonString = "File not found.";
  }

  return reply;
}


// EOF
