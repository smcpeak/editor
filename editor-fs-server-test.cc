// editor-fs-server-test.cc
// Code for editor-fs-server-test.h.

#include "editor-fs-server-test.h"     // this module

// smqtutil
#include "qtutil.h"                    // toString(QString)

// smbase
#include "sm-test.h"                   // PVAL
#include "trace.h"                     // traceAddFromEnvVar


FSServerTest::FSServerTest(int argc, char **argv)
  : QCoreApplication(argc, argv),
    m_eventLoop(),
    m_fsQuery()
{
  QObject::connect(&m_fsQuery, &FileSystemQuery::signal_connected,
                   this, &FSServerTest::on_connected);
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
  TRACE("FSServerTest", "  waiting ...");
  m_eventLoop.exec();

  if (m_fsQuery.hasFailed()) {
    xfatal(m_fsQuery.getFailureReason());
  }

  xassert(m_fsQuery.hasReply());
  return m_fsQuery.takeReply();
}


void FSServerTest::runTests(HostName const &hostname)
{
  TRACE("FSServerTest", "runTests");

  m_fsQuery.connect(hostname);
  while (m_fsQuery.isConnecting()) {
    TRACE("FSServerTest", "  connecting ...");
    m_eventLoop.exec();
  }
  if (m_fsQuery.hasFailed()) {
    xfatal(m_fsQuery.getFailureReason());
  }

  runPathTests();
  runEchoTests();
  runFileReadWriteTests();
  runGetDirEntriesTest();

  m_fsQuery.shutdown();
}


void FSServerTest::runPathTests()
{
  cout << "runPathTests\n";

  // Send.
  {
    VFS_FileStatusRequest req;
    req.m_path = "Makefile";
    m_fsQuery.sendRequest(req);
  }

  // Receive.
  std::unique_ptr<VFS_Message> replyMsg(getNextReply());
  VFS_FileStatusReply *reply = replyMsg->asFileStatusReply();
  xassert(reply->m_success);
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
  VFS_Echo const *reply = replyMsg->asEchoC();

  // Verify that the reply is what was sent.
  xassert(reply->m_data == data);
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


void FSServerTest::runEchoTests()
{
  cout << "runEchoTests\n";

  runEchoTest(std::vector<unsigned char>{});
  runEchoTest(std::vector<unsigned char>{0,1,2,3});

  // Character 26 (0x1A) is treated as signalling EOF by the Windows
  // file system layer in text mode, so it's an important case to check.
  runEchoTest(std::vector<unsigned char>{26});

  // Send the SSH escape sequence that disconnects.  This should be
  // simply passed through as-is without interpretation.
  runEchoTest(std::vector<unsigned char>{'\n', '~', '.'});

  runEchoTest(allBytes());

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


void FSServerTest::runFileReadWriteTests()
{
  cout << "runFileReadWriteTests\n";

  std::vector<unsigned char> data(allBytes());
  string fname = "efst.tmp";
  int64_t modTime;

  // Write.
  {
    VFS_WriteFileRequest req;
    req.m_path = fname;
    req.m_contents = data;
    m_fsQuery.sendRequest(req);

    std::unique_ptr<VFS_Message> replyMsg(getNextReply());
    VFS_WriteFileReply const *reply = replyMsg->asWriteFileReplyC();
    if (reply->m_success) {
      modTime = reply->m_fileModificationTime;
      PVAL(modTime);
    }
    else {
      xfatal(reply->m_failureReasonString);
    }
  }

  // Read.
  {
    VFS_ReadFileRequest req;
    req.m_path = fname;
    m_fsQuery.sendRequest(req);

    std::unique_ptr<VFS_Message> replyMsg(getNextReply());
    VFS_ReadFileReply const *reply = replyMsg->asReadFileReplyC();
    if (reply->m_success) {
      xassert(reply->m_contents == data);
      xassert(reply->m_fileModificationTime == modTime);
      xassert(reply->m_readOnly == false);
    }
    else {
      xfatal(reply->m_failureReasonString);
    }
  }

  // Delete.
  {
    VFS_DeleteFileRequest req;
    req.m_path = fname;
    m_fsQuery.sendRequest(req);

    std::unique_ptr<VFS_Message> replyMsg(getNextReply());
    VFS_DeleteFileReply const *reply = replyMsg->asDeleteFileReplyC();
    if (!reply->m_success) {
      xfatal(reply->m_failureReasonString);
    }
  }

  // Check deletion.
  {
    VFS_FileStatusRequest req;
    req.m_path = fname;
    m_fsQuery.sendRequest(req);

    std::unique_ptr<VFS_Message> replyMsg(getNextReply());
    VFS_FileStatusReply const *reply = replyMsg->asFileStatusReplyC();
    xassert(reply->m_fileKind == SMFileUtil::FK_NONE);
  }

  // Read non-existent.
  {
    VFS_ReadFileRequest req;
    req.m_path = fname;
    m_fsQuery.sendRequest(req);

    std::unique_ptr<VFS_Message> replyMsg(getNextReply());
    VFS_ReadFileReply const *reply = replyMsg->asReadFileReplyC();
    xassert(!reply->m_success);
    xassert(reply->m_failureReasonCode == xSysError::R_FILE_NOT_FOUND);
  }
}


void FSServerTest::runGetDirEntriesTest()
{
  VFS_GetDirEntriesRequest req;
  req.m_path = ".";
  m_fsQuery.sendRequest(req);

  std::unique_ptr<VFS_Message> replyMsg(getNextReply());
  VFS_GetDirEntriesReply const *reply = replyMsg->asGetDirEntriesReplyC();
  xassert(reply->m_success);
  cout << "number of entries: " << reply->m_entries.size() << "\n";

  // Print the first 10 entries.
  for (size_t i=0; i < 10 && i < reply->m_entries.size(); i++) {
    SMFileUtil::DirEntryInfo const &info = reply->m_entries.at(i);
    cout << "name=" << info.m_name
         << " kind=" << toString(info.m_kind)
         << "\n";
  }
}


void FSServerTest::on_connected() NOEXCEPT
{
  m_eventLoop.exit();
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

    HostName hostname(HostName::asLocal());

    QStringList args = fsServerTest.arguments();
    if (args.size() >= 2) {
      hostname = HostName::asSSH(toString(args.at(1)));
      cout << "Running test with hostname: " << hostname << endl;
    }

    fsServerTest.runTests(hostname);
  }
  catch (xBase &x) {
    cerr << x.why() << endl;
    return 2;
  }

  return 0;
}


// EOF
