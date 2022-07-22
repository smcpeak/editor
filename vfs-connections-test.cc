// vfs-connections-test.cc
// Code for vfs-connections-test.h.

#include "vfs-connections-test.h"      // this module

// editor
#include "vfs-msg.h"                   // VFS_Echo

// smbase
#include "exc.h"                       // xBase
#include "trace.h"                     // traceAddFromEnvVar

// libc++
#include <utility>                     // std::move


VFS_ConnectionsTest::VFS_ConnectionsTest(int argc, char **argv)
  : QCoreApplication(argc, argv),
    m_eventLoop(),
    m_vfsConnections()
{
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_connected,
                   this, &VFS_ConnectionsTest::on_connected);
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_replyAvailable,
                   this, &VFS_ConnectionsTest::on_replyAvailable);
  QObject::connect(&m_vfsConnections, &VFS_Connections::signal_vfsConnectionLost,
                   this, &VFS_ConnectionsTest::on_vfsConnectionLost);
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


VFS_Connections::RequestID VFS_ConnectionsTest::sendEchoRequest()
{
  VFS_Connections::RequestID requestID = 0;

  std::unique_ptr<VFS_Echo> req(new VFS_Echo);
  req->m_data = allBytes();
  m_vfsConnections.issueRequest(requestID /*OUT*/,
    HostName::asLocal(), std::move(req));
  cout << "sent echo request: " << requestID << endl;

  return requestID;
}


void VFS_ConnectionsTest::waitForReply(
  VFS_Connections::RequestID requestID)
{
  while (m_vfsConnections.requestIsPending(requestID)) {
    cout << "waiting for reply " << requestID << "\n";
    m_eventLoop.exec();
  }
  if (!m_vfsConnections.replyIsAvailable(requestID)) {
    xfatal(stringb("reply " << requestID < " not available"));
  }
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

  // Send request.
  VFS_Connections::RequestID requestID = sendEchoRequest();

  // Wait for reply.
  waitForReply(requestID);

  // Process reply.
  receiveEchoReply(requestID);
}


void VFS_ConnectionsTest::testMultipleEchos(int howMany)
{
  cout << "testMultipleEchos " << howMany << "\n";

  std::vector<VFS_Connections::RequestID> requestIDs;

  // Enqueue a bunch at once.
  for (int i=0; i < howMany; i++) {
    requestIDs.push_back(sendEchoRequest());
  }

  // Receive them.
  for (int i=0; i < howMany; i++) {
    waitForReply(requestIDs.at(i));
    receiveEchoReply(requestIDs.at(i));
  }
}


void VFS_ConnectionsTest::testCancel(bool wait)
{
  cout << "testCancel wait=" << wait << "\n";

  VFS_Connections::RequestID requestID = sendEchoRequest();

  if (wait) {
    waitForReply(requestID);
  }

  m_vfsConnections.cancelRequest(requestID);
  cout << "cancelled request " << requestID << "\n";
}


void VFS_ConnectionsTest::runTests()
{
  m_vfsConnections.connectLocal();
  while (m_vfsConnections.localIsConnecting()) {
    cout << "waiting for local connection\n";
    m_eventLoop.exec();
  }
  if (!m_vfsConnections.localIsReady()) {
    xfatal("local connection not ready");
  }

  testOneEcho();
  testMultipleEchos(2);
  testMultipleEchos(3);
  testMultipleEchos(10);

  testCancel(false /*wait*/);
  testOneEcho();

  testCancel(true /*wait*/);
  testOneEcho();

  cout << "vfs-connections-test passed\n";
  m_vfsConnections.shutdownAll();
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


void VFS_ConnectionsTest::on_vfsConnectionLost(
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
  catch (xBase &x) {
    cerr << x.why() << endl;
    return 2;
  }

  return 0;
}


// EOF
