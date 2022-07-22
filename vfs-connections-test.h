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

public:      // methods
  VFS_ConnectionsTest(int argc, char **argv);
  ~VFS_ConnectionsTest();

  // Send a single VFS_Echo request, returning the request ID.
  VFS_Connections::RequestID sendEchoRequest();

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
  void on_vfsConnectionLost(HostName hostName, string reason) NOEXCEPT;
};


#endif // EDITOR_VFS_CONNECTIONS_TEST_H
