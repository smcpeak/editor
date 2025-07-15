// lsp-client.cc
// Code for `lsp-client.h`.

#include "lsp-client.h"                // this module

#include "command-runner.h"            // CommandRunner

#include "smqtutil/qtutil.h"           // toString(QString)

#include "smbase/container-util.h"     // smbase::contains
#include "smbase/exc.h"                // smbase::xformat
#include "smbase/gdvalue-json.h"       // gdv::{gdvToJSON, jsonToGDV}
#include "smbase/gdvalue-parse.h"      // gdv::{checkIsMap, checkIsSmallInteger}
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/list-util.h"          // smbase::listMoveFront
#include "smbase/map-util.h"           // mapMoveValueAt, keySet
#include "smbase/overflow.h"           // safeToInt
#include "smbase/parsestring.h"        // ParseString
#include "smbase/set-util.h"           // smbase::setRemoveExisting
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote, trimWhitespace, split
#include "smbase/stringb.h"            // stringb

#include <exception>                   // std::exception
#include <limits>                      // std::numeric_limits
#include <optional>                    // std::make_optional
#include <string_view>                 // std::string_view
#include <utility>                     // std::move
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-client");


/*static*/ char const *LSPClient::toString(MessageParseResult res)
{
  RETURN_ENUMERATION_STRING_OR(
    MessageParseResult,
    NUM_MESSAGE_PARSE_RESULTS,
    (
      // These three should not be seen by the user.
      "extracted one message",
      "prior protocol error",
      "empty data",

      // These may be seen.
      "The headers did not end with a blank line.",
      "A header line lacked a terminating newline.",
      "The body ended before the specified Content-Length.",
    ),
    res,
    "(invalid MessageParseResult)"
  )
}


int LSPClient::innerGetNextRequestID()
{
  // If we hit the maximum, wrap back to 1.
  if (m_nextRequestID == std::numeric_limits<int>::max()) {
    m_nextRequestID = 1;
  }

  return m_nextRequestID++;
}


int LSPClient::getNextRequestID()
{
  // If the ID we want to use is already outstanding, skip it.  (This
  // should be very rare.)
  int iters = 0;
  while (contains(m_outstandingRequests, m_nextRequestID)) {
    innerGetNextRequestID();

    // Safety check.
    xassert(++iters < 1000);
  }

  return innerGetNextRequestID();
}


bool LSPClient::idInUse(int id) const
{
  return contains(m_outstandingRequests, id) ||
         contains(m_pendingReplies, id);
}


void LSPClient::send(std::string &&data)
{
  m_child.putInputData(QByteArray::fromStdString(data));
}


/*static*/ std::string LSPClient::serializeMessage(GDValue const &msg)
{
  std::string msgJSON = gdvToJSON(msg);

  return stringb(
    "Content-Length: " << msgJSON.size() << "\r\n\r\n" <<
    msgJSON);
}


/*static*/ std::string LSPClient::makeRequest(
  int id,
  std::string_view method,
  GDValue const &params)
{
  return serializeMessage(GDVMap{
    { "jsonrpc", "2.0" },
    { "id", id },
    { "method", GDValue(method) },
    { "params", params },
  });
}


/*static*/ std::string LSPClient::makeNotificationBody(
  std::string_view method,
  GDValue const &params)
{
  return serializeMessage(GDVMap{
    { "jsonrpc", "2.0" },
    { "method", method },
    { "params", params },
  });
}


void LSPClient::setProtocolError(std::string &&msg)
{
  if (!hasProtocolError()) {
    TRACE1("protocol error: " << msg);

    m_protocolError = std::make_optional<std::string>(std::move(msg));

    Q_EMIT signal_hasProtocolError();
  }

  else {
    TRACE1("second or later protocol error: " << msg);
  }
}


// Diagnosing specific problems with partial messages upon termination
// is not terribly important (since early termination is a problem
// regardless of the data that was sent before), but it provides a
// convenient way for me to test, in `lsp-client-test`, that each of the
// cases below is being exercised.
auto LSPClient::innerProcessOutputData() -> MessageParseResult
{
  // We can't do anything once a protocol error occurs.
  if (hasProtocolError()) {
    TRACE2("ipod: have protocol error");
    return MPR_PRIOR_ERROR;
  }

  if (!m_child.hasOutputData()) {
    TRACE2("ipod: no data");
    return MPR_EMPTY;
  }

  // Prepare to parse the current output data.
  ParseString parser(m_child.peekOutputData().toStdString());

  // Scan the headers to get the length of the body.  Normally, there is
  // exactly one header line, specifying the length; any other headers
  // are parsed but otherwise ignored.
  std::size_t contentLength = 0;
  while (true) {
    if (parser.eos()) {
      TRACE2("ipod: unterminated headers");
      return MPR_UNTERMINATED_HEADERS;
    }

    std::string line = parser.getUpToByte('\n');
    if (!endsWith(line, "\n")) {
      TRACE2("ipod: unterminated header line");
      return MPR_UNTERMINATED_HEADER_LINE;
    }

    if (line == "\r\n" || line == "\n") {
      // End of headers.
      break;
    }

    if (beginsWith(stringTolower(line), "content-length:")) {
      contentLength = parseDecimalInt_noSign(
        trimWhitespace(split(line, ':').at(1)));
      if (contentLength == 0) {
        xformat("Content-Length value was zero.");
      }
    }
    else {
      // Some other header, ignore.
    }
  }

  if (contentLength == 0) {
    xformat("No Content-Length header in message.");
  }

  // Extract the body.
  std::string bodyJSON = parser.getUpToSize(contentLength);
  if (bodyJSON.size() < contentLength) {
    // Incomplete.
    TRACE2("ipod: incomplete body");
    return MPR_INCOMPLETE_BODY;
  }

  GDValue msg(jsonToGDV(bodyJSON));
  checkIsMap(msg);

  // If it has an "id" field then it is a reply.
  if (msg.mapContains("id")) {
    GDValue gdvId = msg.mapGetValueAt("id");

    // Make sure the value is an integer in the proper range.
    checkIsSmallInteger(gdvId);
    int id = safeToInt(gdvId.smallIntegerGet());
    formatAssert(id >= 0);

    TRACE2("ipod: received reply with ID " << id);

    setRemoveExisting(m_outstandingRequests, id);
    mapInsertUnique(m_pendingReplies, id, std::move(msg));

    Q_EMIT signal_hasReplyForID(id);
  }

  else {
    TRACE2("ipod: received notification");

    m_pendingNotifications.push_back(std::move(msg));

    Q_EMIT signal_hasPendingNotifications();
  }

  // Remove the decoded message from the output data bytes queue.
  TRACE2("ipod: removing " << parser.curOffset() << " bytes of data");
  m_child.removeOutputData(parser.curOffset());

  return MPR_ONE_MESSAGE;
}


void LSPClient::processOutputData() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE2("processOutputData");

  try {
    // There could be multiple messages waiting, so loop until we have
    // processed them all.
    while (innerProcessOutputData() == MPR_ONE_MESSAGE)
      {}
  }
  catch (std::exception &x) {
    setProtocolError(x.what());
  }

  GENERIC_CATCH_END
}


void LSPClient::on_errorDataReady() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("on_errorDataReady");

  Q_EMIT signal_hasErrorData();

  GENERIC_CATCH_END
}


void LSPClient::on_processTerminated() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("on_processTerminated");

  xassert(!isChildRunning());

  // It's possible we could learn about the child terminating before
  // having completely drained the output queue.  And, we want to check
  // for it having exited after writing a partial message.
  while (m_child.hasOutputData()) {
    MessageParseResult res = innerProcessOutputData();
    if (res == MPR_ONE_MESSAGE) {
      // Extracted another message; keep draining the queue.
      continue;
    }

    // This is excluded by the `hasOutputData()` check.
    xassert(res != MPR_EMPTY);

    if (res == MPR_PRIOR_ERROR) {
      // A protocol error has already been recorded, we don't need to do
      // any more diagnosis here.
      break;
    }

    // Any other condition corresponds to the child exiting after having
    // written an incomplete message, so diagnose that.
    setProtocolError(stringb(
      "Server process terminated with an incomplete message: " << toString(res)));
    break;
  }

  // Relay the termination signal to our client.
  Q_EMIT signal_childProcessTerminated();

  GENERIC_CATCH_END
}


LSPClient::~LSPClient()
{
  QObject::disconnect(&m_child, nullptr, this, nullptr);
}


LSPClient::LSPClient(CommandRunner &child)
  : m_child(child),
    m_nextRequestID(1),
    m_outstandingRequests(),
    m_pendingReplies(),
    m_pendingNotifications(),
    m_protocolError()
{
  QObject::connect(&child, &CommandRunner::signal_outputDataReady,
                   this, &LSPClient::processOutputData);
  QObject::connect(&child, &CommandRunner::signal_processTerminated,
                   this, &LSPClient::on_processTerminated);
}


/*static*/ std::string LSPClient::makeFileURI(std::string_view fname)
{
  SMFileUtil sfu;

  std::string absFname = sfu.getAbsolutePath(std::string(fname));

  absFname = sfu.normalizePathSeparators(absFname);

  // In the URI format, a path like "C:/Windows" gets written
  // "/C:/Windows".
  if (!beginsWith(absFname, "/")) {
    absFname = std::string("/") + absFname;
  }

  return stringb("file://" << absFname);
}


void LSPClient::sendNotification(
  std::string_view method,
  GDValue const &params)
{
  xassert(!hasProtocolError());

  std::string body = makeNotificationBody(method, params);

  TRACE1("Sending notification: " << body);

  send(std::move(body));
}


bool LSPClient::hasPendingNotifications() const
{
  return !m_pendingNotifications.empty();
}


int LSPClient::numPendingNotifications() const
{
  return safeToInt(m_pendingNotifications.size());
}


GDValue LSPClient::takeNextNotification()
{
  xassert(hasPendingNotifications());

  return listMoveFront(m_pendingNotifications);
}


int LSPClient::sendRequest(
  std::string_view method,
  gdv::GDValue const &params)
{
  xassert(!hasProtocolError());

  int const id = getNextRequestID();
  xassert(id > 0);

  std::string body = makeRequest(id, method, params);

  TRACE1("Sending request: " << body);

  setInsertUnique(m_outstandingRequests, id);
  send(std::move(body));

  return id;
}


bool LSPClient::hasReplyForID(int id) const
{
  return contains(m_pendingReplies, id);
}


std::set<int> LSPClient::getOutstandingRequestIDs() const
{
  return m_outstandingRequests;
}


std::set<int> LSPClient::getPendingRequestIDs() const
{
  return keySet(m_pendingReplies);
}


GDValue LSPClient::takeReplyForID(int id)
{
  xassert(hasReplyForID(id));

  return mapMoveValueAt(m_pendingReplies, id);
}


bool LSPClient::hasProtocolError() const
{
  return m_protocolError.has_value();
}


std::string LSPClient::getProtocolError() const
{
  xassert(hasProtocolError());
  return m_protocolError.value();
}


bool LSPClient::isChildRunning() const
{
  return m_child.isRunning();
}


bool LSPClient::hasErrorData() const
{
  return m_child.hasErrorData();
}


QByteArray LSPClient::takeErrorData()
{
  return m_child.takeErrorData();
}


// EOF
