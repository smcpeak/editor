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


void FSServerTest::sendRequest(VFS_Message const &msg)
{
  // Serialize the message.
  std::string serMessage;
  {
    std::ostringstream oss;
    StreamFlatten flat(&oss);
    msg.serialize(flat);
    serMessage = oss.str();
  }

  // Get its length.
  unsigned char lenBuf[4];
  uint32_t serMsgLen;
  convertWithoutLoss(serMsgLen, serMessage.size());
  serializeIntNBO(lenBuf, serMsgLen);

  // Combine the length and serialized message into an envelope.
  QByteArray envelope(serMsgLen+4, 0);
  memcpy(envelope.data(), lenBuf, 4);
  memcpy(envelope.data()+4, serMessage.data(), serMsgLen);

  // Send that to the child process.
  TRACE("FSServerTest", "sending message, len=" << serMsgLen);
  if (tracingSys("FSServerTest_detail")) {
    printQByteArray(envelope, "envelope bytes");
  }
  m_commandRunner.putInputData(envelope);
}


bool FSServerTest::haveCompleteReply(
  std::unique_ptr<VFS_Message> &replyMessage,
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
  replyMessage.reset(VFS_Message::deserialize(flat));
  return true;
}


std::unique_ptr<VFS_Message> FSServerTest::getNextReply()
{
  TRACE("FSServerTest", "getNextReply");

  QByteArray replyBytes;
  QByteArray errorBytes;

  std::unique_ptr<VFS_Message> replyMessage;
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


void FSServerTest::runTests()
{
  TRACE("FSServerTest", "runTests");

  m_commandRunner.startAsynchronous();

  runPathTests();
  runEchoTests();

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


void FSServerTest::runPathTests()
{
  // Send.
  {
    VFS_PathRequest req;
    req.m_path = "Makefile";
    sendRequest(req);
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
  sendRequest(request);

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

  // This doesn't work yet, I think because stdin and stdout are open in
  // text mode.
  if (false) {
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
