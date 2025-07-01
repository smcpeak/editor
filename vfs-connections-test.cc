// vfs-connections-test.cc
// Code for vfs-connections-test.h.

#include "vfs-connections-test.h"      // this module

// editor
#include "vfs-msg.h"                   // VFS_Echo

// smqtutil
#include "smqtutil/qtutil.h"           // toString(QString)

// smbase
#include "smbase/exc.h"                // smbase::XBase, xfatal
#include "smbase/trace.h"              // traceAddFromEnvVar

// libc++
#include <utility>                     // std::move

using namespace smbase;


VFS_ConnectionsTest::VFS_ConnectionsTest(int argc, char **argv)
  : QCoreApplication(argc, argv),
    m_eventLoop(),
    m_vfsConnections(),
    m_primaryHostName(HostName::asLocal()),
    m_secondaryHostName(HostName::asLocal())
{
  // If a command line argument is supplied, treat it as an SSH host
  // name.
  if (arguments().size() >= 2) {
    m_secondaryHostName = HostName::asSSH(toString(arguments().at(1)));
  }

  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_connected,
                   this, &VFS_ConnectionsTest::on_connected);
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_replyAvailable,
                   this, &VFS_ConnectionsTest::on_replyAvailable);
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_failed,
                   this, &VFS_ConnectionsTest::on_failed);
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
    cout << "waiting for connection to " << hostName << endl;
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
  cout << "sent echo request: host=" << hostName
       << " id=" << requestID << endl;

  return requestID;
}


void VFS_ConnectionsTest::waitForReply(
  VFS_Connections::RequestID requestID)
{
  while (m_vfsConnections.requestIsPending(requestID)) {
    cout << "waiting for reply " << requestID << "\n";
    xassert(m_vfsConnections.numPendingRequests() > 0);
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
  cout << "testOneEcho\n";

  m_vfsConnections.selfCheck();

  xassert(m_vfsConnections.numPendingRequests() == 0);
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

  xassert(m_vfsConnections.numPendingRequests() == 0);
  xassert(m_vfsConnections.numAvailableReplies() == 0);
}


void VFS_ConnectionsTest::testMultipleEchos(int howMany)
{
  cout << "testMultipleEchos " << howMany << "\n";

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
  cout << "testCancel wait=" << wait << "\n";

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
  cout << "canceled request " << primaryRequestID << "\n";

  if (usingSecondary()) {
    m_vfsConnections.cancelRequest(secondaryRequestID);
    cout << "canceled request " << secondaryRequestID << "\n";
  }
}


void VFS_ConnectionsTest::runTests()
{
  m_vfsConnections.selfCheck();

  cout << "runTests: primary=" << m_primaryHostName;
  if (usingSecondary()) {
    cout << " secondary=" << m_secondaryHostName;
  }
  cout << endl;

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

  cout << "primary start dir: "
       << m_vfsConnections.getStartingDirectory(m_primaryHostName)
       << "\n";
  if (usingSecondary()) {
    cout << "secondary start dir: "
         << m_vfsConnections.getStartingDirectory(m_secondaryHostName)
         << "\n";
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

  cout << "vfs-connections-test passed\n";
}


void VFS_ConnectionsTest::on_connected(HostName hostName) NOEXCEPT
{
  cout << "connected to " << hostName << endl;
  m_eventLoop.exit();
}


void VFS_ConnectionsTest::on_replyAvailable(
  VFS_Connections::RequestID requestID) NOEXCEPT
{
  cout << "got reply: " << requestID << endl;
  m_eventLoop.exit();
}


void VFS_ConnectionsTest::on_failed(
  HostName hostName, string reason) NOEXCEPT
{
  cout << "connection lost: host=" << hostName
       << " reason: " << reason << endl;
  m_eventLoop.exit();
}


int main(int argc, char **argv)
{
  traceAddFromEnvVar();

  try {
    VFS_ConnectionsTest connsTest(argc, argv);
    connsTest.runTests();
  }
  catch (XBase &x) {
    cerr << x.why() << endl;
    return 2;
  }

  return 0;
}


// EOF
