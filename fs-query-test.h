// fs-query-test.h
// Tests for fs-query module.

#ifndef EDITOR_FS_QUERY_TEST_H
#define EDITOR_FS_QUERY_TEST_H

// editor
#include "fs-query.h"                  // FileSystemQuery

// smbase
#include "sm-macros.h"                 // NO_OBJECT_COPIES

// qt
#include <QCoreApplication>
#include <QEventLoop>


// App instance for running tests.
class FSQTest : public QCoreApplication {
  Q_OBJECT
  NO_OBJECT_COPIES(FSQTest);

public:      // data
  // Event loop object used to wait for results to be available.
  QEventLoop m_eventLoop;

  // True if we have received the results from the most recent request.
  bool m_gotResults;

  // The query we have issued.
  FileSystemQuery *m_fsq;

public:      // methods
  FSQTest(int argc, char **argv);
  ~FSQTest();

  // Run the sequence of tests.
  void runTests();

  // Run a test of a local query with the given simulated delay.
  void testLocalQuery(int delayMS);

  // Wait until the pending request has results ready.
  void waitForResults();

public Q_SLOTS:
  // Handler for FileSystemQuery signal.
  void on_resultsReady() NOEXCEPT;
};


#endif // EDITOR_FS_QUERY_TEST_H
