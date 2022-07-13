// editor-fs-server-test.cc
// Code for editor-fs-server-test.h.

#include "editor-fs-server-test.h"     // this module

// editor
#include "command-runner.h"            // CommandRunner
#include "vfs-msg.h"                   // VFS_XXX

// smqtutil
#include "qtutil.h"                    // toString(QString)

// smbase
#include "bflatten.h"                  // StreamFlatten
#include "exc.h"                       // xBase, xFormat
#include "flatten.h"                   // serializeIntNBO
#include "overflow.h"                  // convertWithoutLoss
#include "sm-test.h"                   // PVAL
#include "trace.h"                     // traceAddFromEnvVar

// libc++
#include <sstream>                     // std::i/ostringstream


FSServerTest::FSServerTest(int argc, char **argv)
  : QCoreApplication(argc, argv),
    m_eventLoop(),
    m_commandRunner()
{
  m_commandRunner.setProgram("editor-fs-server.exe");

  QObject::connect(&m_commandRunner, &CommandRunner::signal_outputDataReady,
                   this, &FSServerTest::on_outputDataReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_errorDataReady,
                   this, &FSServerTest::on_errorDataReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_processTerminated,
                   this, &FSServerTest::on_processTerminated);
}


FSServerTest::~FSServerTest()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_commandRunner, nullptr, this, nullptr);
}


void FSServerTest::runTests()
{
  TRACE("FSServerTest", "runTests");

  m_commandRunner.startAsynchronous();

  // Send.
  {
    // Serialize a request.
    std::string serRequest;
    {
      VFS_PathRequest req("Makefile");
      std::ostringstream oss;
      StreamFlatten flat(&oss);
      req.xfer(flat);
      serRequest = oss.str();
    }

    // Get its length.
    unsigned char lenBuf[4];
    uint32_t serReqLen;
    convertWithoutLoss(serReqLen, serRequest.size());
    serializeIntNBO(lenBuf, serReqLen);

    // Combine the length and request into a message.
    QByteArray message(serReqLen+4, 0);
    memcpy(message.data(), lenBuf, 4);
    memcpy(message.data()+4, serRequest.data(), serReqLen);

    // Send that message to the child process.
    TRACE("FSServerTest", "sending message, len=" << serReqLen);
    if (tracingSys("FSServerTest_detail")) {
      printQByteArray(message, "request bytes");
    }
    m_commandRunner.putInputData(message);
  }

  // Receive.
  VFS_PathReply reply(getNextReply());
  PVAL(reply.m_dirName);
  PVAL(reply.m_fileName);
  PVAL(reply.m_dirExists);
  PVAL(reply.m_fileKind);
  PVAL(reply.m_fileModificationTime);

  // Close the server's input and wait for it to stop.
  //
  // TODO: Put a timeout on this.
  TRACE("FSServerTest", "closing input channel");
  m_commandRunner.closeInputChannel();
  while (m_commandRunner.isRunning()) {
    TRACE("FSServerTest", "  waiting for server termination");
    m_eventLoop.exec();
  }
  TRACE("FSServerTest", "  server terminated");
}


bool FSServerTest::haveCompleteReply(VFS_PathReply &replyMessage,
                                     QByteArray const &replyBytes)
{
  uint32_t replyBytesSize = replyBytes.size();
  if (replyBytesSize < 4) {
    return false;
  }

  unsigned char lenBuf[4];
  memcpy(lenBuf, replyBytes.data(), 4);
  uint32_t replyLen;
  deserializeIntNBO(lenBuf, replyLen);

  if (replyBytesSize < 4+replyLen) {
    return false;
  }

  if (replyBytesSize > 4+replyLen) {
    xformat(stringb(
      "Reply has extra bytes.  Its size is " << replyBytesSize <<
      " but the expected size is " << (4+replyLen) << "."));
  }

  if (tracingSys("FSServerTest_detail")) {
    printQByteArray(replyBytes, "reply bytes");
  }

  std::string messageBytes(replyBytes.data()+4, replyLen);
  std::istringstream iss(messageBytes);
  StreamFlatten flat(&iss);
  replyMessage.xfer(flat);
  return true;
}


VFS_PathReply FSServerTest::getNextReply()
{
  TRACE("FSServerTest", "getNextReply");

  QByteArray replyBytes;
  QByteArray errorBytes;

  VFS_PathReply replyMessage;
  while (!haveCompleteReply(replyMessage, replyBytes) &&
         m_commandRunner.isRunning()) {
    // Wait for something to happen.
    TRACE("FSServerTest", "  waiting...");
    m_eventLoop.exec();

    // Accumulate any resulting data.
    if (m_commandRunner.hasOutputData()) {
      TRACE("FSServerTest", "    ... got output data");
      replyBytes.append(m_commandRunner.takeOutputData());
    }
    if (m_commandRunner.hasErrorData()) {
      TRACE("FSServerTest", "    ... got error data");
      errorBytes.append(m_commandRunner.takeErrorData());
    }
  }

  if (!m_commandRunner.isRunning()) {
    stringBuilder sb;
    sb << "editor-fs-server terminated unexpectedly: ";
    sb << toString(m_commandRunner.getTerminationDescription());
    if (!errorBytes.isEmpty()) {
      sb << "  stderr: " << errorBytes.toStdString();
    }
    throw xFormat(sb.str());
  }

  TRACE("FSServerTest", "getNextReply returning");
  return replyMessage;
}


void FSServerTest::on_outputDataReady() NOEXCEPT
{
  m_eventLoop.exit();
}

void FSServerTest::on_errorDataReady() NOEXCEPT
{
  m_eventLoop.exit();
}

void FSServerTest::on_processTerminated() NOEXCEPT
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
