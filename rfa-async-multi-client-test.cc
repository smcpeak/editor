// rfa-async-multi-client-test.cc
// Code for rfa-async-multi-client-test.h.

#include "unit-tests.h"                          // decl for my entry point
#include "rfa-async-multi-client-test.h"         // this module

// editor
#include "rfa-msg.h"                             // RFA_Msg_Echo

// smqtutil
#include "smqtutil/qtutil.h"                     // toString(QString)

// smbase
#include "smbase/exc.h"                          // smbase::XBase, xfatal
#include "smbase/sm-test.h"                      // DIAG
#include "smbase/trace.h"                        // traceAddFromEnvVar

// libc++
#include <utility>                               // std::move

using namespace smbase;


RFA_AsyncMultiClientTest::RFA_AsyncMultiClientTest(CmdlineArgsSpan args)
  : QObject(),
    m_eventLoop(),
    m_rfaAMClient(),
    m_primaryHostName(HostName::asLocal()),
    m_secondaryHostName(HostName::asLocal())
{
  // If a command line argument is supplied, treat it as an SSH host
  // name.
  if (!args.empty()) {
    m_secondaryHostName = HostName::asSSH(args[0]);
  }

  QObject::connect(
    &m_rfaAMClient, &RFA_AsyncMultiClient::signal_rfaConnected,
    this,           &RFA_AsyncMultiClientTest::on_rfaConnected);
  QObject::connect(
    &m_rfaAMClient, &RFA_AsyncMultiClient::signal_rfaReplyAvailable,
    this,           &RFA_AsyncMultiClientTest::on_rfaReplyAvailable);
  QObject::connect(
    &m_rfaAMClient, &RFA_AsyncMultiClient::signal_rfaFailed,
    this,           &RFA_AsyncMultiClientTest::on_rfaFailed);
}


RFA_AsyncMultiClientTest::~RFA_AsyncMultiClientTest()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_rfaAMClient, nullptr, this, nullptr);
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


void RFA_AsyncMultiClientTest::waitForConnection(HostName const &hostName)
{
  while (m_rfaAMClient.isConnecting(hostName)) {
    DIAG("waiting for connection to " << hostName);
    m_eventLoop.exec();
  }
  if (!m_rfaAMClient.isReady(hostName)) {
    RFA_AbstractAsyncMultiClient::ConnectionState cs =
      m_rfaAMClient.connectionState(hostName);
    xfatal("connection to " << hostName << " not ready; "
           "its state is " << toString(cs));
  }
}


RFA_AsyncMultiClient::RequestID
  RFA_AsyncMultiClientTest::sendEchoRequest(HostName const &hostName)
{
  RFA_AsyncMultiClient::RequestID requestID = 0;

  std::unique_ptr<RFA_Msg_Echo> req(new RFA_Msg_Echo);
  req->m_data = allBytes();
  m_rfaAMClient.issueRequest(requestID /*OUT*/,
    hostName, std::move(req));
  DIAG("sent echo request: host=" << hostName <<
       " id=" << requestID);

  return requestID;
}


void RFA_AsyncMultiClientTest::waitForReply(
  RFA_AsyncMultiClient::RequestID requestID)
{
  while (m_rfaAMClient.requestIsOutstanding(requestID)) {
    DIAG("waiting for reply " << requestID);
    xassert(m_rfaAMClient.numOutstandingRequests() > 0);
    m_eventLoop.exec();
  }
  if (!m_rfaAMClient.replyIsAvailable(requestID)) {
    xfatal("reply " << requestID << " not available");
  }

  xassert(m_rfaAMClient.numAvailableReplies() > 0);
}


void RFA_AsyncMultiClientTest::receiveEchoReply(
  RFA_AsyncMultiClient::RequestID requestID)
{
  std::unique_ptr<RFA_Message> reply(
    m_rfaAMClient.takeReply(requestID));
  RFA_Msg_Echo const *echoReply = reply->asEchoC();
  xassert(echoReply->m_data == allBytes());
}


void RFA_AsyncMultiClientTest::testOneEcho()
{
  DIAG("testOneEcho");

  m_rfaAMClient.selfCheck();

  xassert(m_rfaAMClient.numOutstandingRequests() == 0);
  xassert(m_rfaAMClient.numAvailableReplies() == 0);

  // Send requests.
  RFA_AsyncMultiClient::RequestID primaryRequestID =
    sendEchoRequest(m_primaryHostName);
  RFA_AsyncMultiClient::RequestID secondaryRequestID = 0;
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

  xassert(m_rfaAMClient.numOutstandingRequests() == 0);
  xassert(m_rfaAMClient.numAvailableReplies() == 0);
}


void RFA_AsyncMultiClientTest::testMultipleEchos(int howMany)
{
  DIAG("testMultipleEchos " << howMany);

  std::vector<RFA_AsyncMultiClient::RequestID> requestIDs;

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
  for (RFA_AsyncMultiClient::RequestID id : requestIDs) {
    waitForReply(id);
    receiveEchoReply(id);
  }
}


void RFA_AsyncMultiClientTest::testCancel(bool wait)
{
  DIAG("testCancel wait=" << wait);

  RFA_AsyncMultiClient::RequestID primaryRequestID =
    sendEchoRequest(m_primaryHostName);
  RFA_AsyncMultiClient::RequestID secondaryRequestID = 0;
  if (usingSecondary()) {
    secondaryRequestID = sendEchoRequest(m_secondaryHostName);
  }

  if (wait) {
    waitForReply(primaryRequestID);
    if (usingSecondary()) {
      waitForReply(secondaryRequestID);
    }
  }

  m_rfaAMClient.cancelRequest(primaryRequestID);
  DIAG("canceled request " << primaryRequestID);

  if (usingSecondary()) {
    m_rfaAMClient.cancelRequest(secondaryRequestID);
    DIAG("canceled request " << secondaryRequestID);
  }
}


void RFA_AsyncMultiClientTest::runTests()
{
  m_rfaAMClient.selfCheck();

  DIAG("runTests: primary=" << m_primaryHostName);
  if (usingSecondary()) {
    DIAG("  secondary=" << m_secondaryHostName);
  }

  m_rfaAMClient.connect(m_primaryHostName);
  if (usingSecondary()) {
    m_rfaAMClient.connect(m_secondaryHostName);
  }

  waitForConnection(m_primaryHostName);
  if (usingSecondary()) {
    waitForConnection(m_secondaryHostName);
  }

  xassert(m_rfaAMClient.isOrWasConnected(m_primaryHostName));
  if (usingSecondary()) {
    xassert(m_rfaAMClient.isOrWasConnected(m_secondaryHostName));
  }

  DIAG("primary start dir: " <<
       m_rfaAMClient.getStartingDirectory(m_primaryHostName));
  if (usingSecondary()) {
    DIAG("secondary start dir: " <<
         m_rfaAMClient.getStartingDirectory(m_secondaryHostName));
  }

  testOneEcho();
  testMultipleEchos(2);
  testMultipleEchos(3);
  testMultipleEchos(10);

  testCancel(false /*wait*/);
  testOneEcho();

  testCancel(true /*wait*/);
  testOneEcho();

  m_rfaAMClient.selfCheck();
  m_rfaAMClient.shutdownAll();
  m_rfaAMClient.selfCheck();
}


void RFA_AsyncMultiClientTest::on_rfaConnected(HostName hostName) NOEXCEPT
{
  DIAG("connected to " << hostName);
  m_eventLoop.exit();
}


void RFA_AsyncMultiClientTest::on_rfaReplyAvailable(
  RFA_AsyncMultiClient::RequestID requestID) NOEXCEPT
{
  DIAG("got reply: " << requestID);
  m_eventLoop.exit();
}


void RFA_AsyncMultiClientTest::on_rfaFailed(
  HostName hostName, string reason) NOEXCEPT
{
  // The test infrastructure doesn't have a better way to report
  // connection errors than just printing them and then letting the
  // harness notice the connection isn't ready.
  std::cout <<
    "connection lost: host=" << hostName <<
    " reason: " << reason << std::endl;

  m_eventLoop.exit();
}


// Called from unit-tests.cc.
void test_vfs_connections(CmdlineArgsSpan args)
{
  RFA_AsyncMultiClientTest connsTest(args);
  connsTest.runTests();
}


// EOF
