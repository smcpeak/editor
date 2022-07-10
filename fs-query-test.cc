// fs-query-test.cc
// Code for fs-query-test.h.

#include "fs-query-test.h"             // this module

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "sm-test.h"                   // PVAL
#include "trace.h"                     // TRACE


FSQTest::FSQTest(int argc, char **argv)
  : QCoreApplication(argc, argv),
    m_eventLoop(),
    m_gotResults(false),
    m_fsq(nullptr)
{}


FSQTest::~FSQTest()
{
  TRACE_MS("FSQTest", "~FSQTest");

  if (m_fsq) {
    QObject::disconnect(m_fsq, nullptr, this, nullptr);
    m_fsq->cancelRequest();
    delete m_fsq;
  }
}


void FSQTest::runTests()
{
  TRACE_MS("FSQTest", "runTests");

  testLocalQuery(200 /*delayMS*/);
  testLocalQuery(0 /*delayMS*/);
  testLocalQuery(50 /*delayMS*/);
}


void FSQTest::testLocalQuery(int delayMS)
{
  TRACE_MS("FSQTest", "testLocalQuery(" << delayMS << ")");

  m_fsq = new FileSystemQuery();
  m_fsq->m_simulatedDelayMS = delayMS;
  QObject::connect(m_fsq, &FileSystemQuery::signal_resultsReady,
                   this, &FSQTest::on_resultsReady);

  m_gotResults = false;
  m_fsq->queryPath("named-td.h");

  waitForResults();

  PVAL(m_fsq->m_dirExists);
  PVAL(m_fsq->m_baseKind);
  PVAL(m_fsq->m_baseModificationTime);
}


void FSQTest::waitForResults()
{
  TRACE_MS("FSQTest", "waitForResults started");

  if (!m_gotResults) {
    m_eventLoop.exec();
  }

  TRACE_MS("FSQTest", "waitForResults finished");
}


void FSQTest::on_resultsReady() NOEXCEPT
{
  TRACE_MS("FSQTest", "on_resultsReady");

  GENERIC_CATCH_BEGIN

  m_gotResults = true;
  m_eventLoop.exit();

  GENERIC_CATCH_END
}


int main(int argc, char **argv)
{
  traceAddFromEnvVar();

  FSQTest fsqTest(argc, argv);
  fsqTest.runTests();

  return 0;
}


// EOF
