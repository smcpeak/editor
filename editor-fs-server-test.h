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

  // Run the sequence of tests.
  void runTests();

  // If 'replyBytes' contains a complete message, deserialize it into
  // 'replyMessage' and return true.  Otherwise return false.
  bool haveCompleteReply(VFS_PathReply &replyMessage,
                         QByteArray const &replyBytes);

  // Wait for a complete reply and return it.  Throw on error.
  VFS_PathReply getNextReply();

public Q_SLOTS:
  // Handlers for CommandRunner signals.
  void on_outputDataReady() NOEXCEPT;
  void on_errorDataReady() NOEXCEPT;
  void on_processTerminated() NOEXCEPT;
};


#endif // EDITOR_EDITOR_FS_SERVER_TEST_H
