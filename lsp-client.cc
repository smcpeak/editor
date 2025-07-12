// lsp-client.cc
// Code for `lsp-client.h`.

#include "lsp-client.h"                // this module

#include "command-runner.h"            // CommandRunner

#include "smqtutil/qtutil.h"           // toString(QString)

#include "smbase/exc.h"                // smbase::xformat
#include "smbase/gdvalue-json.h"       // gdv::gdvToJSON, jsonToGDV
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote, trimWhitespace, split
#include "smbase/stringb.h"            // stringb

#include <string_view>                 // std::string_view
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-client");


void LSPClient::send(std::string const &data)
{
  m_child.putInputData(QByteArray::fromStdString(data));
}


/*static*/ std::string LSPClient::serializeMessage(GDValue const &msg)
{
  TRACE1("Preparing to send: " << msg);

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


/*static*/ std::string LSPClient::makeNotification(
  std::string_view method,
  GDValue const &params)
{
  return serializeMessage(GDVMap{
    { "jsonrpc", "2.0" },
    { "method", method },
    { "params", params },
  });
}


GDValue LSPClient::readResponse()
{
  TRACE1("Waiting for response ...");

  // Accumulate headers.
  std::string headers = "";
  while (true) {
    TRACE2("Waiting for next header line ...");
    std::string chunk = toString(m_child.waitForOutputLine());
    TRACE2("Got header line: " << doubleQuote(chunk));
    if (chunk == "\r\n" || chunk == "\n") {
      break;
    }
    if (chunk.empty()) {
      xformat("Unexpected EOF in readResponse.");
    }
    headers += chunk;
  }

  // Look for the Content-Length header.
  int contentLength = 0;
  std::vector<std::string> lines = split(headers, '\n');
  for (std::string const &line : lines) {
    if (beginsWith(stringTolower(line), "content-length:")) {
      contentLength = parseDecimalInt_noSign(
        trimWhitespace(split(line, ':').at(1)));
    }
  }
  if (contentLength == 0) {
    xformat("No Content-Length header in response.");
  }

  TRACE2("Waiting for " << contentLength << " bytes of body data ...");
  std::string json = m_child.waitForOutputData(contentLength).toStdString();

  GDValue ret(jsonToGDV(json));
  TRACE1("Received response: " << ret);

  return ret;
}


GDValue LSPClient::readResponsesUntil(int id)
{
  GDValue resp(readResponse());

  while (!( resp.mapContains("id") &&
            resp.mapGetValueAt("id") == GDValue(id) )) {
    std::cout << "Some other response:\n"
              << resp.asLinesString();

    resp = readResponse();
  }

  std::cout << "Got response with id=" << id << ":\n"
            << resp.asLinesString();

  return resp;
}


LSPClient::~LSPClient()
{}


LSPClient::LSPClient(CommandRunner &child)
  : m_child(child),
    m_nextRequestID(1)
{}


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


GDValue LSPClient::sendRequestPrintReply(
  std::string_view method,
  GDValue const &params)
{
  int id = m_nextRequestID++;

  send(makeRequest(id, method, params));

  GDValue resp(readResponsesUntil(id));

  return resp;
}


void LSPClient::sendNotification(
  std::string_view method,
  GDValue const &params)
{
  send(makeNotification(method, params));
}




// EOF
