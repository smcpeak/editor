// editor-fs-server-test.h
// Test program for editor-fs-server.

#ifndef EDITOR_EDITOR_FS_SERVER_TEST_H
#define EDITOR_EDITOR_FS_SERVER_TEST_H

// editor
#include "host-name.h"                 // HostName
#include "rfa-async-single-client.h"   // RFA_AsyncSingleClient
#include "rfa-msg.h"                   // RFA_Msg_FileStatusReply

// smbase
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES

// qt
#include <QEventLoop>
#include <QObject>

// libc++
#include <memory>                      // std::unique_ptr


// App instance for running tests.
class FSServerTest : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(FSServerTest);

public:      // data
  // Event loop object used to wait for results to be available.
  QEventLoop m_eventLoop;

  // Query manager object.
  RFA_AsyncSingleClient m_rfaASClient;

public:      // methods
  FSServerTest();
  ~FSServerTest();

  // Wait for and return the next reply, or throw on error.
  std::unique_ptr<RFA_Message> getNextReply();

  // Run the sequence of tests.
  void runTests(HostName const &hostName);

  // Establish a connection to 'hostName'.
  void connect(HostName const &hostName);

  // Issue a path query using FileStatusRequest.
  void runPathQuery(string const &path);

  // Use Echo to send/receive 'data'.
  void runEchoTest(std::vector<unsigned char> const &data);

  // Use the Echo message to test the ability to send and receive
  // various patterns of data.
  void runEchoTests();

  // Run tests related to reading and writing file contents.
  void runFileReadWriteTests();

  // Test the GetDirEntries request and reply.
  void runGetDirEntriesTest();

public Q_SLOTS:
  // Handlers for RFA_AsyncSingleClient signals.
  void on_rfaConnected() NOEXCEPT;
  void on_rfaReplyAvailable() NOEXCEPT;
  void on_rfaFailureAvailable() NOEXCEPT;
};


#endif // EDITOR_EDITOR_FS_SERVER_TEST_H
