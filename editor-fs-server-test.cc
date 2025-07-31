// editor-fs-server-test.cc
// Code for editor-fs-server-test.h.

#include "editor-fs-server-test.h"     // this module
#include "unit-tests.h"                // decl for my entry point

// smqtutil
#include "smqtutil/qtutil.h"           // toString(QString)

// smbase
#include "smbase/exc.h"                // smbase::XBase
#include "smbase/sm-test.h"            // DIAG, VPVAL
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/syserr.h"             // smbase::XSysError

using namespace smbase;


FSServerTest::FSServerTest()
  : QObject(),
    m_eventLoop(),
    m_fsQuery()
{
  QObject::connect(&m_fsQuery, &VFS_FileSystemQuery::signal_vfsConnected,
                   this, &FSServerTest::on_vfsConnected);
  QObject::connect(&m_fsQuery, &VFS_FileSystemQuery::signal_vfsReplyAvailable,
                   this, &FSServerTest::on_vfsReplyAvailable);
  QObject::connect(&m_fsQuery, &VFS_FileSystemQuery::signal_vfsFailureAvailable,
                   this, &FSServerTest::on_vfsFailureAvailable);
}


FSServerTest::~FSServerTest()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_fsQuery, nullptr, this, nullptr);
}


std::unique_ptr<VFS_Message> FSServerTest::getNextReply()
{
  DIAG("getNextReply");

  // Wait for something to happen.
  DIAG("  waiting ...");
  m_eventLoop.exec();

  if (m_fsQuery.hasFailed()) {
    xfatal(m_fsQuery.getFailureReason());
  }

  xassert(m_fsQuery.hasReply());
  return m_fsQuery.takeReply();
}


void FSServerTest::runTests(HostName const &hostName)
{
  DIAG("runTests");

  connect(hostName);

  runPathQuery("Makefile");
  runEchoTests();
  runFileReadWriteTests();
  runGetDirEntriesTest();

  m_fsQuery.shutdown();
}


void FSServerTest::connect(HostName const &hostName)
{
  m_fsQuery.connect(hostName);
  while (m_fsQuery.isConnecting()) {
    DIAG("connecting ...");
    m_eventLoop.exec();
  }
  if (m_fsQuery.hasFailed()) {
    xfatal(m_fsQuery.getFailureReason());
  }
}


void FSServerTest::runPathQuery(string const &path)
{
  DIAG("runPathQuery(" << doubleQuote(path) << ")");

  // Send.
  {
    VFS_FileStatusRequest req;
    req.m_path = path;
    m_fsQuery.sendRequest(req);
  }

  // Receive.
  std::unique_ptr<VFS_Message> replyMsg(getNextReply());
  VFS_FileStatusReply *reply = replyMsg->asFileStatusReply();
  xassert(reply->m_success);
  VPVAL(reply->m_dirName);
  VPVAL(reply->m_fileName);
  VPVAL(reply->m_dirExists);
  VPVAL(reply->m_fileKind);
  VPVAL(reply->m_fileModificationTime);
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
  DIAG("runEchoTests");

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
  DIAG("runFileReadWriteTests");

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
      VPVAL(modTime);
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
    xassert(reply->m_failureReasonCode == XSysError::R_FILE_NOT_FOUND);
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
  DIAG("number of entries: " << reply->m_entries.size());

  // Print the first 10 entries.
  for (size_t i=0; i < 10 && i < reply->m_entries.size(); i++) {
    SMFileUtil::DirEntryInfo const &info = reply->m_entries.at(i);
    DIAG("name=" << info.m_name <<
         " kind=" << toString(info.m_kind));
  }
}


void FSServerTest::on_vfsConnected() NOEXCEPT
{
  m_eventLoop.exit();
}

void FSServerTest::on_vfsReplyAvailable() NOEXCEPT
{
  m_eventLoop.exit();
}

void FSServerTest::on_vfsFailureAvailable() NOEXCEPT
{
  m_eventLoop.exit();
}


// Called from unit-tests.cc.
void test_editor_fs_server(CmdlineArgsSpan args)
{
  FSServerTest fsServerTest;

  HostName hostname(HostName::asLocal());

  if (!args.empty()) {
    // TODO: This test is broken.  It's evidently been that way a while.
    hostname = HostName::asSSH(args[0]);
    DIAG("Running test with hostname: " << hostname);

    if (args.size() >= 2) {
      string path = toString(args[1]);
      fsServerTest.connect(hostname);
      fsServerTest.runPathQuery(path);
      fsServerTest.m_fsQuery.shutdown();
      return;
    }
  }

  fsServerTest.runTests(hostname);
}


// EOF
