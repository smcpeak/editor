// rfa-async-multi-client-test.h
// Tests for rfa-async-multi-client.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_ASYNC_MULTI_CLIENT_TEST_H
#define EDITOR_RFA_ASYNC_MULTI_CLIENT_TEST_H

#include "unit-tests.h"                // CmdlineArgsSpan
#include "rfa-async-multi-client.h"    // module under test

// smbase
#include "smbase/str.h"                // string

// qt
#include <QEventLoop>
#include <QObject>


// Test RFA_AsyncMultiClient.
class RFA_AsyncMultiClientTest : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(RFA_AsyncMultiClientTest);

public:      // data
  // Event loop object used to wait for results to be available.
  QEventLoop m_eventLoop;

  // Object to test.
  RFA_AsyncMultiClient m_rfaAMClient;

  // Primary host to connect to.
  HostName m_primaryHostName;

  // Optional secondary host to also connect to, interleaving
  // communication with it and the primary.  Only active if not local.
  HostName m_secondaryHostName;

public:      // methods
  RFA_AsyncMultiClientTest(CmdlineArgsSpan args);
  ~RFA_AsyncMultiClientTest();

  // True if we are using 'm_secondaryHostName'.
  bool usingSecondary() const { return !m_secondaryHostName.isLocal(); }

  // Wait until the connection to 'hostName' is ready.
  void waitForConnection(HostName const &hostName);

  // Send a single RFA_Echo request, returning the request ID.
  RFA_AsyncMultiClient::RequestID sendEchoRequest(HostName const &hostName);

  // Wait until a given reply is available.
  void waitForReply(RFA_AsyncMultiClient::RequestID requestID);

  // Receive an available echo reply.
  void receiveEchoReply(RFA_AsyncMultiClient::RequestID requestID);

  // Test sending and receiving a single echo request.
  void testOneEcho();

  // Test sending a bunch at once, then receiving them all.
  void testMultipleEchos(int howMany);

  // Issue a request and then cancel it.
  void testCancel(bool wait);

  // Run all tests.
  void runTests();

public Q_SLOTS:
  // Handlers for RFA_AsyncMultiClient signals.
  void on_rfaConnected(HostName hostName) NOEXCEPT;
  void on_rfaReplyAvailable(RFA_AsyncMultiClient::RequestID requestID) NOEXCEPT;
  void on_rfaFailed(HostName hostName, string reason) NOEXCEPT;
};


#endif // EDITOR_RFA_ASYNC_MULTI_CLIENT_TEST_H
