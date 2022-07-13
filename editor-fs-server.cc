// editor-fs-server.cc
// Program to serve virtual file system requests.

#include "vfs-local.h"                 // VFS_LocalImpl

#include "bflatten.h"                  // StreamFlatten
#include "exc.h"                       // xfatal
#include "flatten.h"                   // de/serializeIntNBO
#include "overflow.h"                  // convertWithoutLoss
#include "sm-file-util.h"              // SMFileUtil
#include "string-utils.h"              // doubleQuote
#include "syserr.h"                    // xsyserror

#include <fstream>                     // std::ofstream
#include <sstream>                     // std::i/ostringstream
#include <string>                      // std::string
#include <vector>                      // std::vector


// If not null, stream to log to.
static std::ofstream *logStream = nullptr;

#define LOG(stuff)                    \
  if (logStream) {                    \
    *logStream << stuff << std::endl; \
  }


// Read 'size' bytes from 'stream'.  Return false on EOF.  If we do not
// get an immediate EOF, but still fail to read 'size' bytes, throw.
static bool freadAll(void *ptr, size_t size, FILE *stream)
{
  LOG("freadAll(size=" << size << ")");

  size_t res = fread(ptr, 1, size, stream);
  LOG("  fread returned " << res);

  if (res == 0) {
    // Clean EOF.
    return false;
  }

  if (res < size) {
    if (feof(stream)) {
      xfatal(stringb(
        "Unexpected end of input; got " << res <<
        " bytes, expected " << size << "."));
    }
    else {
      xsyserror("read");
    }
  }

  xassert(res == size);
  return true;
}


// Write 'size' bytes to 'stream'.
static void fwriteAll(void const *ptr, size_t size, FILE *stream)
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
static std::string receiveMessage(FILE *stream)
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
static void sendMessage(FILE *stream, std::string const &reply)
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


static int innerMain()
{
  VFS_LocalImpl localImpl;

  while (true) {
    // Get the next serialized request.
    std::string requestData = receiveMessage(stdin);
    if (requestData.empty()) {
      // No more requests.
      break;
    }
    LOG("requestData: " << doubleQuote(requestData));

    // Deserialize the request.
    std::istringstream iss(requestData);
    StreamFlatten flatInput(&iss);
    VFS_PathRequest pathRequest(flatInput);
    pathRequest.xfer(flatInput);

    // Process it.
    VFS_PathReply pathReply(localImpl.queryPath(pathRequest));

    // Serialize the reply.
    std::ostringstream oss;
    StreamFlatten flatOutput(&oss);
    pathReply.xfer(flatOutput);

    // Send it.
    std::string replyData = oss.str();
    LOG("replyData: " << doubleQuote(replyData));
    sendMessage(stdout, replyData);
  }

  return 0;
}


int main()
{
  try {
    SMFileUtil sfu;
    sfu.createDirectoryAndParents("out");
    logStream = new std::ofstream("out/fs-server.log");
    LOG("editor-fs-server started");

    int ret = innerMain();

    if (logStream) {
      LOG("editor-fs-server terminating with code " << ret);
    }
    return ret;
  }
  catch (xBase &x) {
    if (logStream) {
      LOG("editor-fs-server terminating with exception: " << x.why());
    }
    cerr << x.why() << endl;
    return 2;
  }
}


// EOF
