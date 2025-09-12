// json-rpc-client.cc
// Code for `json-rpc-client.h`.

#include "json-rpc-client.h"           // this module

#include "command-runner.h"            // CommandRunner
#include "json-rpc-reply.h"            // JSON_RPC_Reply

#include "smqtutil/qtutil.h"           // toString(QString)

#include "smbase/container-util.h"     // smbase::contains
#include "smbase/exc.h"                // smbase::xformat
#include "smbase/gdvalue-json.h"       // gdv::{gdvToJSON, jsonToGDV}
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/iter-and-end.h"       // smbase::ConstIterAndEnd
#include "smbase/list-util.h"          // smbase::listMoveFront
#include "smbase/map-util.h"           // mapMoveValueAt, keySet
#include "smbase/overflow.h"           // safeToInt
#include "smbase/parsestring.h"        // ParseString
#include "smbase/set-util.h"           // smbase::setRemoveExisting
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-span.h"            // smbase::Span
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote, trimWhitespace, split
#include "smbase/stringb.h"            // stringb
#include "smbase/stringf.h"            // stringf

#include <cstdlib>                     // std::getenv
#include <exception>                   // std::exception
#include <fstream>                     // std::ofstream
#include <limits>                      // std::numeric_limits
#include <optional>                    // std::make_optional
#include <string_view>                 // std::string_view
#include <utility>                     // std::move
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("json-rpc-client");


// -------------------------- JSON_RPC_Client --------------------------
/*static*/ char const *JSON_RPC_Client::toString(MessageParseResult res)
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


int JSON_RPC_Client::innerGetNextRequestID()
{
  // If we hit the maximum, wrap back to 1.
  if (m_nextRequestID == std::numeric_limits<int>::max()) {
    m_nextRequestID = 1;
  }

  return m_nextRequestID++;
}


int JSON_RPC_Client::getNextRequestID()
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


void JSON_RPC_Client::send(std::string &&data)
{
  // Provide a crude mechanism for logging all of the messages.
  static char const *logAllMessagesDir =
    std::getenv("JSON_RPC_CLIENT_SEND_LOG_DIR");
  if (logAllMessagesDir) {
    static int messageNumber = 1;

    std::string fname =
      stringf("%s/msg%04d.bin", logAllMessagesDir, messageNumber);
    std::ofstream logFile(fname.c_str(), std::ios::binary);
    logFile.write(data.data(), data.size());

    ++messageNumber;
  }

  m_child.putInputData(QByteArray::fromStdString(data));
}


/*static*/ std::string JSON_RPC_Client::serializeMessage(GDValue const &msg)
{
  std::string msgJSON = gdvToJSON(msg);

  // As a minor convenience for the log file, write a newline after each
  // JSON payload.
  return stringb(
    "Content-Length: " << (msgJSON.size()+1) << "\r\n\r\n" <<
    msgJSON << '\n');
}


/*static*/ std::string JSON_RPC_Client::makeRequest(
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


/*static*/ std::string JSON_RPC_Client::makeNotificationBody(
  std::string_view method,
  GDValue const &params)
{
  return serializeMessage(GDVMap{
    { "jsonrpc", "2.0" },
    { "method", method },
    { "params", params },
  });
}


void JSON_RPC_Client::setProtocolError(std::string &&msg)
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


GDValue JSON_RPC_Client::call_jsonToGDV(std::string const &bodyJSON) const
{
  try {
    return jsonToGDV(bodyJSON);
  }
  catch (std::exception &x) {
    // Facilitate diagnosing the deeper problem by logging the offending
    // JSON.
    if (m_protocolDiagnosticLog) {
      *m_protocolDiagnosticLog
        << "Error while parsing message JSON: " << x.what() << "\n"
        << "Offending JSON text: " << bodyJSON << "\n";
      m_protocolDiagnosticLog->flush();
    }
    throw;
  }
}


// Diagnosing specific problems with partial messages upon termination
// is not terribly important (since early termination is a problem
// regardless of the data that was sent before), but it provides a
// convenient way for me to test, in `json-rpc-client-test`, that each of the
// cases below is being exercised.
auto JSON_RPC_Client::innerProcessOutputData() -> MessageParseResult
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
  ParseString stringParser(m_child.peekOutputData().toStdString());

  // Scan the headers to get the length of the body.  Normally, there is
  // exactly one header line, specifying the length; any other headers
  // are parsed but otherwise ignored.
  std::size_t contentLength = 0;
  while (true) {
    if (stringParser.eos()) {
      TRACE2("ipod: unterminated headers");
      return MPR_UNTERMINATED_HEADERS;
    }

    std::string line = stringParser.getUpToByte('\n');
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
  std::string bodyJSON = stringParser.getUpToSize(contentLength);
  if (bodyJSON.size() < contentLength) {
    // Incomplete.
    TRACE2("ipod: incomplete body");
    return MPR_INCOMPLETE_BODY;
  }

  TRACE3("ipod: bodyJSON: " << bodyJSON);

  GDValue msgValue(call_jsonToGDV(bodyJSON));
  GDValueParser msg(msgValue);
  msg.checkIsMap();

  // If it has an "id" field then it is a reply.
  if (msg.mapContains("id")) {
    GDValueParser gdvId = msg.mapGetValueAt("id");

    // Make sure the value is an integer in the proper range.
    gdvId.checkIsSmallInteger();
    int id = safeToInt(gdvId.smallIntegerGet());
    if (id < 0) {
      gdvId.throwError(stringb("ID is negative: " << id));
    }
    TRACE1("received reply with ID " << id << ": " <<
           msgValue.asIndentedString());

    setRemoveExisting(m_outstandingRequests, id);

    // Error reply?
    if (msg.mapContains("error")) {
      mapInsertUniqueMove(m_pendingReplies, id, JSON_RPC_Reply(
          JSON_RPC_Error::fromProtocol(msg.mapGetValueAt("error"))));
    }

    else {
      // It must have a "result" field.  This call will throw if not.
      msg.mapGetValueAt("result");

      // Get the contained result.
      GDValue &result = msgValue.mapGetValueAt("result");

      // We are done with the parsers, and about to move `msgValue`.
      gdvId.clearParserPointers();
      msg.clearParserPointers();

      // Move just the `result` portion.
      mapInsertUniqueMove(m_pendingReplies, id, JSON_RPC_Reply(
        std::move(result)));
    }

    Q_EMIT signal_hasReplyForID(id);
  }

  else {
    TRACE1("received notification: " << msgValue.asIndentedString());

    msg.clearParserPointers();

    m_pendingNotifications.push_back(std::move(msgValue));

    Q_EMIT signal_hasPendingNotifications();
  }

  // Remove the decoded message from the output data bytes queue.
  TRACE2("ipod: removing " << stringParser.curOffset() << " bytes of data");
  m_child.removeOutputData(stringParser.curOffset());

  return MPR_ONE_MESSAGE;
}


void JSON_RPC_Client::processOutputData() NOEXCEPT
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


void JSON_RPC_Client::on_errorDataReady() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE1("on_errorDataReady");

  Q_EMIT signal_hasErrorData();

  GENERIC_CATCH_END
}


void JSON_RPC_Client::on_processTerminated() NOEXCEPT
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


JSON_RPC_Client::~JSON_RPC_Client()
{
  QObject::disconnect(&m_child, nullptr, this, nullptr);
}


JSON_RPC_Client::JSON_RPC_Client(
  CommandRunner &child,
  std::ostream * NULLABLE protocolDiagnosticLog)
  : IMEMBFP(child),
    IMEMBFP(protocolDiagnosticLog),
    m_nextRequestID(1),
    m_outstandingRequests(),
    m_pendingReplies(),
    m_canceledRequests(),
    m_pendingNotifications(),
    m_protocolError()
{
  QObject::connect(&child, &CommandRunner::signal_outputDataReady,
                   this, &JSON_RPC_Client::processOutputData);
  QObject::connect(&child, &CommandRunner::signal_processTerminated,
                   this, &JSON_RPC_Client::on_processTerminated);
}


void JSON_RPC_Client::selfCheck() const
{
  xassert(m_nextRequestID > 0);

  std::set<int> pendingReplyIDs = mapKeySet(m_pendingReplies);

  // The ID sets should all be mutually disjoint.
  ConstIterAndEnd<std::set<int>> iterAndEnds[] = {
    constIterAndEnd(m_outstandingRequests),
    constIterAndEnd(pendingReplyIDs),
    constIterAndEnd(m_canceledRequests),
  };
  if (!setsAreDisjoint(Span(iterAndEnds))) {
    // The sets were not disjoint.  Diagnose.
    NWayComparisonResult const result =
      compareNSetIterators(Span(iterAndEnds));
    xassert(result.hasEqualIndices());

    auto const [ai, bi] = result.getEqualIndices();
    int const commonElement = *iterAndEnds[ai];

    static char const * const names[] = {
      "m_outstandingRequests",
      "pendingReplyIDs",
      "m_canceledRequests",
    };
    static_assert(TABLESIZE(iterAndEnds) == TABLESIZE(names));

    xassert(cc::z_le_lt(ai, TABLESIZE(names)));
    xassert(cc::z_le_lt(bi, TABLESIZE(names)));

    // I don't currently have any reason to believe I will *need* this
    // extra diagnosis, but it's a nice demonstration of an application
    // of the `compareNSetIterators` function.
    xfailure_stringbc(
      "ID sets should have been mutually disjoint, but " <<
      names[ai] << " and " << names[bi] <<
      " both have " << commonElement << ".");
  }
}


JSON_RPC_Client::operator GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "JSON_RPC_Client"_sym);

  GDV_WRITE_MEMBER_SYM(m_nextRequestID);
  GDV_WRITE_MEMBER_SYM(m_outstandingRequests);
  m.mapSetValueAtSym("numPendingReplies", m_pendingReplies.size());
  GDV_WRITE_MEMBER_SYM(m_canceledRequests);
  m.mapSetValueAtSym("numPendingNotifications",
    m_pendingNotifications.size());
  GDV_WRITE_MEMBER_SYM(m_protocolError);

  return m;
}


void JSON_RPC_Client::sendNotification(
  std::string_view method,
  GDValue const &params)
{
  xassert(!hasProtocolError());

  TRACE1("Sending " << doubleQuote(method) <<
         " notification: " << params.asIndentedString());

  std::string body = makeNotificationBody(method, params);

  send(std::move(body));
}


bool JSON_RPC_Client::hasPendingNotifications() const
{
  return !m_pendingNotifications.empty();
}


int JSON_RPC_Client::numPendingNotifications() const
{
  return safeToInt(m_pendingNotifications.size());
}


GDValue JSON_RPC_Client::takeNextNotification()
{
  xassert(hasPendingNotifications());

  return listMoveFront(m_pendingNotifications);
}


int JSON_RPC_Client::sendRequest(
  std::string_view method,
  gdv::GDValue const &params)
{
  xassert(!hasProtocolError());

  int const id = getNextRequestID();
  xassert(id > 0);

  TRACE1("Sending " << doubleQuote(method) <<
         " request with ID " << id <<
         ": " << params.asIndentedString());

  std::string body = makeRequest(id, method, params);

  setInsertUnique(m_outstandingRequests, id);
  send(std::move(body));

  return id;
}


std::set<int> JSON_RPC_Client::getOutstandingRequestIDs() const
{
  return m_outstandingRequests;
}


bool JSON_RPC_Client::hasReplyForID(int id) const
{
  return contains(m_pendingReplies, id);
}


std::set<int> JSON_RPC_Client::getPendingReplyIDs() const
{
  return mapKeySet(m_pendingReplies);
}


JSON_RPC_Reply JSON_RPC_Client::takeReplyForID(int id)
{
  xassertPrecondition(hasReplyForID(id));

  return mapMoveValueAt(m_pendingReplies, id);
}


void JSON_RPC_Client::cancelRequestWithID(int id)
{
  if (setRemove(m_outstandingRequests, id)) {
    // The request was outstanding, meaning the server will eventually
    // send a reply.  Keep track of it until it does.
    TRACE1("Canceled outstanding reply for request " << id);
    setInsert(m_canceledRequests, id);
  }

  else {
    // If we already have a reply, discard it.
    if (mapRemove(m_pendingReplies, id)) {
      TRACE1("Canceled pending reply for request " << id);
    }
    else {
      // This is unexpected, but probably not worth asserting.
      TRACE1("Cancel attempted for request " << id <<
             " that was neither outstanding nor pending.");
    }
  }
}


bool JSON_RPC_Client::hasProtocolError() const
{
  return m_protocolError.has_value();
}


std::string JSON_RPC_Client::getProtocolError() const
{
  xassert(hasProtocolError());
  return m_protocolError.value();
}


bool JSON_RPC_Client::isChildRunning() const
{
  return m_child.isRunning();
}


bool JSON_RPC_Client::hasErrorData() const
{
  return m_child.hasErrorData();
}


QByteArray JSON_RPC_Client::takeErrorData()
{
  return m_child.takeErrorData();
}


// EOF
