// editor-fs-server-test.h
// Test program for editor-fs-server.

#ifndef EDITOR_EDITOR_FS_SERVER_TEST_H
#define EDITOR_EDITOR_FS_SERVER_TEST_H

// editor
#include "command-runner.h"            // CommandRunner
#include "vfs-msg.h"                   // VFS_PathReply

// smbase
#include "sm-macros.h"                 // NO_OBJECT_COPIES

// qt
#include <QCoreApplication>
#include <QEventLoop>

// libc++
#include <memory>                      // std::unique_ptr


// App instance for running tests.
class FSServerTest : public QCoreApplication {
  Q_OBJECT
  NO_OBJECT_COPIES(FSServerTest);

public:      // data
  // Event loop object used to wait for results to be available.
  QEventLoop m_eventLoop;

  // Runner connected to the server.
  CommandRunner m_commandRunner;

public:
  FSServerTest(int argc, char **argv);
  ~FSServerTest();

  // Send 'msg' to the child process.
  void sendRequest(VFS_Message const &msg);

  // If 'replyBytes' contains a complete message, deserialize it into
  // 'replyMessage' and return true.  Otherwise return false.
  bool haveCompleteReply(
    std::unique_ptr<VFS_Message> &replyMessage,
    QByteArray const &replyBytes);

  // Wait for a complete reply and return it.  Throw on error.
  std::unique_ptr<VFS_Message> getNextReply();

  // Run the sequence of tests.
  void runTests();

  // Run tests using PathRequest.
  void runPathTests();

  // Use Echo to send/receive 'data'.
  void runEchoTest(std::vector<unsigned char> const &data);

  // Use the Echo message to test the ability to send and receive
  // various patterns of data.
  void runEchoTests();

public Q_SLOTS:
  // Handlers for CommandRunner signals.
  void on_outputDataReady() NOEXCEPT;
  void on_errorDataReady() NOEXCEPT;
  void on_processTerminated() NOEXCEPT;
};


#endif // EDITOR_EDITOR_FS_SERVER_TEST_H
