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
#include "smbase/map-util.h"           // mapMoveValueAt
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


void LSPClient::innerProcessOutputData()
{
  // We can't do anything once a protocol error occurs.
  if (m_protocolError.has_value()) {
    return;
  }

  // Prepare to parse the current output data.
  ParseString parser(m_child.peekOutputData().toStdString());

  // Scan the headers to get the length of the body.
  std::size_t contentLength = 0;
  while (true) {
    std::string line = parser.getUpToByte('\n');
    if (!endsWith(line, "\n")) {
      // Incomplete message; wait for more data to arrive.
      return;
    }

    if (line == "\r\n" || line == "\n") {
      // End of headers.
      break;
    }

    if (beginsWith(stringTolower(line), "content-length:")) {
      contentLength = parseDecimalInt_noSign(
        trimWhitespace(split(line, ':').at(1)));
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
    return;
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

    setRemoveExisting(m_outstandingRequests, id);
    mapInsertUnique(m_pendingReplies, id, std::move(msg));

    Q_EMIT signal_hasReplyForID(id);
  }

  else {
    m_pendingNotifications.push_back(std::move(msg));

    Q_EMIT signal_hasPendingNotifications();
  }

  // Remove the decoded message from the output data bytes queue.
  m_child.removeOutputData(parser.curOffset());
}


void LSPClient::processOutputData() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  try {
    innerProcessOutputData();
  }
  catch (std::exception &x) {
    m_protocolError = std::make_optional<std::string>(x.what());

    Q_EMIT signal_hasProtocolError();
  }

  GENERIC_CATCH_END
}


void LSPClient::on_processTerminated() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  xassert(!isChildRunning());

  // Relay the signal to our client.
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

  int id = getNextRequestID();

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


// EOF
