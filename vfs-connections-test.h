// vfs-connections-test.h
// Tests for vfs-connections.

#ifndef EDITOR_VFS_CONNECTIONS_TEST_H
#define EDITOR_VFS_CONNECTIONS_TEST_H

#include "vfs-connections.h"           // module under test

// smbase
#include "str.h"                       // string

// qt
#include <QCoreApplication>
#include <QEventLoop>


// Test VFS_Connections.
class VFS_ConnectionsTest : public QCoreApplication {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_ConnectionsTest);

public:      // data
  // Event loop object used to wait for results to be available.
  QEventLoop m_eventLoop;

  // Object to test.
  VFS_Connections m_vfsConnections;

  // Primary host to connect to.
  HostName m_primaryHostName;

  // Optional secondary host to also connect to, interleaving
  // communication with it and the primary.  Only active if not local.
  HostName m_secondaryHostName;

public:      // methods
  VFS_ConnectionsTest(int argc, char **argv);
  ~VFS_ConnectionsTest();

  // True if we are using 'm_secondaryHostName'.
  bool usingSecondary() const { return !m_secondaryHostName.isLocal(); }

  // Wait until the connection to 'hostName' is ready.
  void waitForConnection(HostName const &hostName);

  // Send a single VFS_Echo request, returning the request ID.
  VFS_Connections::RequestID sendEchoRequest(HostName const &hostName);

  // Wait until a given reply is available.
  void waitForReply(VFS_Connections::RequestID requestID);

  // Receive an available echo reply.
  void receiveEchoReply(VFS_Connections::RequestID requestID);

  // Test sending and receiving a single echo request.
  void testOneEcho();

  // Test sending a bunch at once, then receiving them all.
  void testMultipleEchos(int howMany);

  // Issue a request and then cancel it.
  void testCancel(bool wait);

  // Run all tests.
  void runTests();

public Q_SLOTS:
  // Handlers for VFS_Connections signals.
  void on_connected(HostName hostName) NOEXCEPT;
  void on_replyAvailable(VFS_Connections::RequestID requestID) NOEXCEPT;
  void on_failed(HostName hostName, string reason) NOEXCEPT;
};


#endif // EDITOR_VFS_CONNECTIONS_TEST_H
