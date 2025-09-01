// vfs-connections-test.cc
// Code for vfs-connections-test.h.

#include "unit-tests.h"                // decl for my entry point
#include "vfs-connections-test.h"      // this module

// editor
#include "vfs-msg.h"                   // VFS_Echo

// smqtutil
#include "smqtutil/qtutil.h"           // toString(QString)

// smbase
#include "smbase/exc.h"                // smbase::XBase, xfatal
#include "smbase/sm-test.h"            // DIAG
#include "smbase/trace.h"              // traceAddFromEnvVar

// libc++
#include <utility>                     // std::move

using namespace smbase;


VFS_ConnectionsTest::VFS_ConnectionsTest(CmdlineArgsSpan args)
  : QObject(),
    m_eventLoop(),
    m_vfsConnections(),
    m_primaryHostName(HostName::asLocal()),
    m_secondaryHostName(HostName::asLocal())
{
  // If a command line argument is supplied, treat it as an SSH host
  // name.
  if (!args.empty()) {
    m_secondaryHostName = HostName::asSSH(args[0]);
  }

  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_vfsConnected,
                   this, &VFS_ConnectionsTest::on_vfsConnected);
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_vfsReplyAvailable,
                   this, &VFS_ConnectionsTest::on_vfsReplyAvailable);
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_vfsFailed,
                   this, &VFS_ConnectionsTest::on_vfsFailed);
}


VFS_ConnectionsTest::~VFS_ConnectionsTest()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_vfsConnections, nullptr, this, nullptr);
}


// Build a vector that has every individual byte.
static std::vector<unsigned char> allBytes()
{
  std::vector<unsigned char> data;
  for (int value=0; value < 256; value++) {
    data.push_back((unsigned char)value);
  }
  return data;
}


void VFS_ConnectionsTest::waitForConnection(HostName const &hostName)
{
  while (m_vfsConnections.isConnecting(hostName)) {
    DIAG("waiting for connection to " << hostName);
    m_eventLoop.exec();
  }
  if (!m_vfsConnections.isReady(hostName)) {
    xfatal("connection to " << hostName << " not ready");
  }
}


VFS_Connections::RequestID
  VFS_ConnectionsTest::sendEchoRequest(HostName const &hostName)
{
  VFS_Connections::RequestID requestID = 0;

  std::unique_ptr<VFS_Echo> req(new VFS_Echo);
  req->m_data = allBytes();
  m_vfsConnections.issueRequest(requestID /*OUT*/,
    hostName, std::move(req));
  DIAG("sent echo request: host=" << hostName <<
       " id=" << requestID);

  return requestID;
}


void VFS_ConnectionsTest::waitForReply(
  VFS_Connections::RequestID requestID)
{
  while (m_vfsConnections.requestIsOutstanding(requestID)) {
    DIAG("waiting for reply " << requestID);
    xassert(m_vfsConnections.numOutstandingRequests() > 0);
    m_eventLoop.exec();
  }
  if (!m_vfsConnections.replyIsAvailable(requestID)) {
    xfatal("reply " << requestID << " not available");
  }

  xassert(m_vfsConnections.numAvailableReplies() > 0);
}


void VFS_ConnectionsTest::receiveEchoReply(
  VFS_Connections::RequestID requestID)
{
  std::unique_ptr<VFS_Message> reply(
    m_vfsConnections.takeReply(requestID));
  VFS_Echo const *echoReply = reply->asEchoC();
  xassert(echoReply->m_data == allBytes());
}


void VFS_ConnectionsTest::testOneEcho()
{
  DIAG("testOneEcho");

  m_vfsConnections.selfCheck();

  xassert(m_vfsConnections.numOutstandingRequests() == 0);
  xassert(m_vfsConnections.numAvailableReplies() == 0);

  // Send requests.
  VFS_Connections::RequestID primaryRequestID =
    sendEchoRequest(m_primaryHostName);
  VFS_Connections::RequestID secondaryRequestID = 0;
  if (usingSecondary()) {
    secondaryRequestID = sendEchoRequest(m_secondaryHostName);
  }

  // Wait for first reply.
  waitForReply(primaryRequestID);

  // Process first reply.
  receiveEchoReply(primaryRequestID);

  if (usingSecondary()) {
    // Wait for and process second reply.
    waitForReply(secondaryRequestID);
    receiveEchoReply(secondaryRequestID);
  }

  xassert(m_vfsConnections.numOutstandingRequests() == 0);
  xassert(m_vfsConnections.numAvailableReplies() == 0);
}


void VFS_ConnectionsTest::testMultipleEchos(int howMany)
{
  DIAG("testMultipleEchos " << howMany);

  std::vector<VFS_Connections::RequestID> requestIDs;

  // Enqueue a bunch at once.
  for (int i=0; i < howMany; i++) {
    // When 'i' is odd, enqueue the request to the secondary first so
    // we alternate which one goes first.
    bool odd = (i % 2 == 1);

    if (odd && usingSecondary()) {
      requestIDs.push_back(sendEchoRequest(m_secondaryHostName));
    }

    requestIDs.push_back(sendEchoRequest(m_primaryHostName));

    if (!odd && usingSecondary()) {
      requestIDs.push_back(sendEchoRequest(m_secondaryHostName));
    }
  }

  // Receive them.
  for (VFS_Connections::RequestID id : requestIDs) {
    waitForReply(id);
    receiveEchoReply(id);
  }
}


void VFS_ConnectionsTest::testCancel(bool wait)
{
  DIAG("testCancel wait=" << wait);

  VFS_Connections::RequestID primaryRequestID =
    sendEchoRequest(m_primaryHostName);
  VFS_Connections::RequestID secondaryRequestID = 0;
  if (usingSecondary()) {
    secondaryRequestID = sendEchoRequest(m_secondaryHostName);
  }

  if (wait) {
    waitForReply(primaryRequestID);
    if (usingSecondary()) {
      waitForReply(secondaryRequestID);
    }
  }

  m_vfsConnections.cancelRequest(primaryRequestID);
  DIAG("canceled request " << primaryRequestID);

  if (usingSecondary()) {
    m_vfsConnections.cancelRequest(secondaryRequestID);
    DIAG("canceled request " << secondaryRequestID);
  }
}


void VFS_ConnectionsTest::runTests()
{
  m_vfsConnections.selfCheck();

  DIAG("runTests: primary=" << m_primaryHostName);
  if (usingSecondary()) {
    DIAG("  secondary=" << m_secondaryHostName);
  }

  m_vfsConnections.connect(m_primaryHostName);
  if (usingSecondary()) {
    m_vfsConnections.connect(m_secondaryHostName);
  }

  waitForConnection(m_primaryHostName);
  if (usingSecondary()) {
    waitForConnection(m_secondaryHostName);
  }

  xassert(m_vfsConnections.isOrWasConnected(m_primaryHostName));
  if (usingSecondary()) {
    xassert(m_vfsConnections.isOrWasConnected(m_secondaryHostName));
  }

  DIAG("primary start dir: " <<
       m_vfsConnections.getStartingDirectory(m_primaryHostName));
  if (usingSecondary()) {
    DIAG("secondary start dir: " <<
         m_vfsConnections.getStartingDirectory(m_secondaryHostName));
  }

  testOneEcho();
  testMultipleEchos(2);
  testMultipleEchos(3);
  testMultipleEchos(10);

  testCancel(false /*wait*/);
  testOneEcho();

  testCancel(true /*wait*/);
  testOneEcho();

  m_vfsConnections.selfCheck();
  m_vfsConnections.shutdownAll();
  m_vfsConnections.selfCheck();
}


void VFS_ConnectionsTest::on_vfsConnected(HostName hostName) NOEXCEPT
{
  DIAG("connected to " << hostName);
  m_eventLoop.exit();
}


void VFS_ConnectionsTest::on_vfsReplyAvailable(
  VFS_Connections::RequestID requestID) NOEXCEPT
{
  DIAG("got reply: " << requestID);
  m_eventLoop.exit();
}


void VFS_ConnectionsTest::on_vfsFailed(
  HostName hostName, string reason) NOEXCEPT
{
  DIAG("connection lost: host=" << hostName <<
       " reason: " << reason);
  m_eventLoop.exit();
}


// Called from unit-tests.cc.
void test_vfs_connections(CmdlineArgsSpan args)
{
  VFS_ConnectionsTest connsTest(args);
  connsTest.runTests();
}


// EOF
