// editor-fs-server-test.cc
// Code for editor-fs-server-test.h.

#include "editor-fs-server-test.h"     // this module

// smbase
#include "sm-test.h"                   // PVAL
#include "trace.h"                     // traceAddFromEnvVar


FSServerTest::FSServerTest(int argc, char **argv)
  : QCoreApplication(argc, argv),
    m_eventLoop(),
    m_fsQuery()
{
  QObject::connect(&m_fsQuery, &FileSystemQuery::signal_replyAvailable,
                   this, &FSServerTest::on_replyAvailable);
  QObject::connect(&m_fsQuery, &FileSystemQuery::signal_failureAvailable,
                   this, &FSServerTest::on_failureAvailable);
}


FSServerTest::~FSServerTest()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_fsQuery, nullptr, this, nullptr);
}


std::unique_ptr<VFS_Message> FSServerTest::getNextReply()
{
  TRACE("FSServerTest", "getNextReply");

  // Wait for something to happen.
  TRACE("FSServerTest", "  waiting...");
  m_eventLoop.exec();

  if (m_fsQuery.hasFailed()) {
    xfatal(m_fsQuery.getFailureReason());
  }

  xassert(m_fsQuery.hasReply());
  return m_fsQuery.takeReply();
}


void FSServerTest::runTests()
{
  TRACE("FSServerTest", "runTests");

  runPathTests();
  runEchoTests();

  m_fsQuery.shutdown();
}


void FSServerTest::runPathTests()
{
  // Send.
  {
    VFS_PathRequest req;
    req.m_path = "Makefile";
    m_fsQuery.sendRequest(req);
  }

  // Receive.
  std::unique_ptr<VFS_Message> replyMsg(getNextReply());
  VFS_PathReply *reply = dynamic_cast<VFS_PathReply*>(replyMsg.get());
  xassert(reply);
  PVAL(reply->m_dirName);
  PVAL(reply->m_fileName);
  PVAL(reply->m_dirExists);
  PVAL(reply->m_fileKind);
  PVAL(reply->m_fileModificationTime);
}


void FSServerTest::runEchoTest(std::vector<unsigned char> const &data)
{
  VFS_Echo request;
  request.m_data = data;
  m_fsQuery.sendRequest(request);

  std::unique_ptr<VFS_Message> replyMsg(getNextReply());
  VFS_Echo const *reply = dynamic_cast<VFS_Echo const *>(replyMsg.get());
  xassert(reply);

  // Verify that the reply is what was sent.
  xassert(reply->m_data == data);
}


void FSServerTest::runEchoTests()
{
  runEchoTest(std::vector<unsigned char>{});
  runEchoTest(std::vector<unsigned char>{0,1,2,3});

  // Character 26 (0x1A) is treated as signalling EOF by the Windows
  // file system layer in text mode, so it's an important case to check.
  runEchoTest(std::vector<unsigned char>{26});

  {
    // Build a vector that has every individual byte.
    std::vector<unsigned char> data;
    for (int value=0; value < 256; value++) {
        data.push_back((unsigned char)value);
    }
    runEchoTest(data);
  }

  {
    // Build a vector that has every possible pair of adjacent bytes.
    std::vector<unsigned char> data;
    for (int first=0; first < 256; first++) {
      for (int second=0; second < 256; second++) {
        data.push_back((unsigned char)first);
        data.push_back((unsigned char)second);
      }
    }
    runEchoTest(data);
  }
}


void FSServerTest::on_replyAvailable() NOEXCEPT
{
  m_eventLoop.exit();
}

void FSServerTest::on_failureAvailable() NOEXCEPT
{
  m_eventLoop.exit();
}


int main(int argc, char **argv)
{
  traceAddFromEnvVar();

  try {
    FSServerTest fsServerTest(argc, argv);
    fsServerTest.runTests();
  }
  catch (xBase &x) {
    cerr << x.why() << endl;
    return 2;
  }

  return 0;
}


// EOF
