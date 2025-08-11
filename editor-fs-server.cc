// editor-fs-server.cc
// Program to serve virtual file system requests.

#include "vfs-local.h"                           // VFS_LocalImpl

// smbase
#include "smbase/bflatten.h"                     // StreamFlatten
#include "smbase/binary-stdin.h"                 // setStdinToBinary, setStdoutToBinary
#include "smbase/datetime.h"                     // DateTimeSeconds
#include "smbase/exc.h"                          // xfatal, smbase::XBase
#include "smbase/exclusive-write-file.h"         // smbase::{ExclusiveWriteFile, tryCreateExclusiveWriteFile}
#include "smbase/flatten.h"                      // de/serializeIntNBO
#include "smbase/nonport.h"                      // sleepForMilliseconds, getProcessId
#include "smbase/overflow.h"                     // convertWithoutLoss
#include "smbase/sm-env.h"                       // smbase::{getXDGStateHome, envAsIntOr}
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // NULLABLE, OPEN_ANONYMOUS_NAMESPACE
#include "smbase/string-util.h"                  // doubleQuote
#include "smbase/syserr.h"                       // smbase::xsyserror

// libc++
#include <cstdlib>                               // std::min
#include <fstream>                               // std::ofstream
#include <memory>                                // std::unique_ptr
#include <sstream>                               // std::i/ostringstream
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// NOTE: It is not possible (without some work) to use the `sm-trace`
// module here because the client treats anything appearing on stderr as
// indicative of an error, and stdout carries protocol data.
//INIT_TRACE("editor-fs-server");


// If not null, stream to log to.
//
// This does not use a smart pointer because I want to ensure it still
// exists while handling exceptions, etc.
ExclusiveWriteFile * NULLABLE logStream = nullptr;

// Normal logging.
#define LOG(stuff)                             \
  if (logStream) {                             \
    logStream->stream() << stuff << std::endl; \
  }

// Verbose logging, normally disabled.
#if 1
  #define LOG_VERBOSE(stuff) ((void)0)
#else
  #define LOG_VERBOSE(stuff) LOG(stuff)
#endif


// Read 'size' bytes from 'stream'.  Return false on EOF.  If we do not
// get an immediate EOF, but still fail to read 'size' bytes, throw.
bool freadAll(unsigned char *ptr, size_t size, FILE *stream)
{
  LOG("freadAll(size=" << size << ")");

  size_t totalRead = 0;
  while (totalRead < size) {
    // There is a strange problem when reading too much data at once if
    // we are running under `ssh`:
    //
    //   https://stackoverflow.com/questions/79729658/why-does-readfile-on-stdin-with-size-at-least-64kib-hang-under-ssh
    //
    // The workaround is to limit the read size to 32kiB.
    size_t maxToRead = std::min(size - totalRead,
                                static_cast<size_t>(0x8000));

    size_t res = fread(ptr+totalRead, 1, maxToRead, stream);
    LOG("  fread returned " << res);

    if (res == 0) {
      break;
    }

    totalRead += res;
  }

  if (totalRead == 0) {
    // Clean EOF.
    return false;
  }

  if (totalRead < size) {
    if (feof(stream)) {
      xfatal(stringb(
        "Unexpected end of input; got " << totalRead <<
        " bytes, expected " << size << "."));
    }
    else {
      xsyserror("read");
    }
  }

  xassert(totalRead == size);
  return true;
}


// Write 'size' bytes to 'stream'.
void fwriteAll(void const *ptr, size_t size, FILE *stream)
{
  LOG("fwriteAll(size=" << size << ")");

  size_t res = fwrite(ptr, 1, size, stream);
  if (res != size) {
    xsyserror("write");
  }
  fflush(stream);
}


// Read the next request from 'stream'.  A request consists of a 4-byte
// length in network byte order, followed by that many bytes of message
// contents, which are returned from this function as a string.
//
// If there are no more requests (the stream has been closed), return an
// empty string.
std::string receiveMessage(FILE *stream)
{
  // Read the message length.
  unsigned char buf[4];
  if (!freadAll(buf, 4, stream)) {
    return "";
  }
  uint32_t len;
  deserializeIntNBO(buf, len);

  // Read the message contents.
  std::vector<unsigned char> message(len);
  if (!freadAll(message.data(), len, stream)) {
    xfatal(stringb(
      "Got EOF when trying to read message with length " << len << "."));
  }

  // In C++17, it is possible to write directly into string data, but I
  // am currently targeting C++14, so I have to construct the string
  // object as a separate step.
  return std::string((char const *)(message.data()), len);
}


// Write the given reply to stdout.  The syntax is the same as for
// requests: 4-byte NBO length, then that many bytes of message data.
void sendMessage(FILE *stream, std::string const &reply)
{
  // Send length.
  uint32_t len;
  convertWithoutLoss(len, reply.size());
  unsigned char buf[4];
  serializeIntNBO(buf, len);
  fwriteAll(buf, 4, stream);

  // Send contents.
  fwriteAll(reply.data(), len, stream);
}


// Send 'msg' as a message on stdout.
void sendReply(VFS_Message const &msg)
{
  // Serialize the reply.
  std::ostringstream oss;
  StreamFlatten flatOutput(&oss);
  msg.serialize(flatOutput);

  // Send it.
  std::string replyData = oss.str();
  LOG_VERBOSE("replyData: " << doubleQuote(replyData));
  sendMessage(stdout, replyData);
}


int innerMain()
{
  VFS_LocalImpl localImpl;

  // Allow an artificial delay to be inserted into message processing
  // for testing purposes.
  unsigned artificialDelay = 0;
  if (char const *d = getenv("EDITOR_FS_SERVER_DELAY")) {
    artificialDelay = (unsigned)atoi(d);
  }

  while (true) {
    // Get the next serialized request.
    std::string requestData = receiveMessage(stdin);
    if (requestData.empty()) {
      // No more requests.
      break;
    }
    LOG_VERBOSE("requestData: " << doubleQuote(requestData));

    // Deserialize the request.
    std::istringstream iss(requestData);
    StreamFlatten flatInput(&iss);
    std::unique_ptr<VFS_Message> message(
      VFS_Message::deserialize(flatInput));

    if (artificialDelay) {
      LOG("sleeping for " << artificialDelay << " ms");
      sleepForMilliseconds(artificialDelay);
    }

    // Process it.
    switch (message->messageType()) {
      default:
        xformat(stringb("Bad message type: " << message->messageType()));

      case VFS_MT_GetVersion:
        // For now, have the server just ignore the incoming version
        // number, and let the client diagnose mismatches.
        sendReply(VFS_GetVersion());
        break;

      case VFS_MT_Echo: {
        VFS_Echo const *echo = message->asEchoC();
        sendReply(*echo);
        break;
      }

      case VFS_MT_FileStatusRequest: {
        VFS_FileStatusRequest const *pathRequest = message->asFileStatusRequestC();
        VFS_FileStatusReply pathReply(localImpl.queryPath(*pathRequest));
        sendReply(pathReply);
        break;
      }

      case VFS_MT_ReadFileRequest:
        sendReply(localImpl.readFile(*(message->asReadFileRequestC())));
        break;

      case VFS_MT_WriteFileRequest:
        sendReply(localImpl.writeFile(*(message->asWriteFileRequestC())));
        break;

      case VFS_MT_DeleteFileRequest:
        sendReply(localImpl.deleteFile(*(message->asDeleteFileRequestC())));
        break;

      case VFS_MT_GetDirEntriesRequest:
        sendReply(localImpl.getDirEntries(*(message->asGetDirEntriesRequestC())));
        break;
    }
  }

  return 0;
}


CLOSE_ANONYMOUS_NAMESPACE


int main()
{
  try {
    // Set up log file.
    SMFileUtil sfu;
    std::string logFileName =
      sfu.normalizePathSeparators(getXDGStateHome()) +
      "/sm-editor/fs-server.log";
    sfu.createParentDirectories(logFileName);
    logStream = tryCreateExclusiveWriteFile(logFileName);

    // Write first log line.
    DateTimeSeconds dts;
    dts.fromCurrentTime();
    LOG("editor-fs-server started at " << dts.dateTimeString() <<
        ", pid=" << getProcessId());

    // Since we are using stdin and stdout as the message channel, it
    // needs to be able to transport arbitrary data.  Windows text mode
    // translation and interpretation interferes with that.
    setStdinToBinary();
    setStdoutToBinary();

    int ret = innerMain();

    if (logStream) {
      LOG("editor-fs-server terminating with code " << ret);
      delete logStream;
      logStream = nullptr;
    }
    return ret;
  }
  catch (XBase &x) {
    if (logStream) {
      LOG("editor-fs-server terminating with exception: " << x.why());
    }
    cerr << x.why() << endl;
    return 2;
  }
}


// EOF
