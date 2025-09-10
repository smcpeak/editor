// json-rpc-client-test.cc
// Tests for `json-rpc-client`.

#include "json-rpc-client-test.h"      // decls for this test module
#include "unit-tests.h"                // decl for my entry point

#include "json-rpc-client.h"           // module under test
#include "json-rpc-reply.h"            // module under test

#include "command-runner.h"            // CommandRunner
#include "uri-util.h"                  // makeFileURI

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/exc.h"                // smbase::XBase
#include "smbase/gdvalue-either.h"     // gdv::toGDValue(smbase::Either)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, RETURN_ENUMERATION_STRING_OR
#include "smbase/sm-span.h"            // smbase::Span
#include "smbase/sm-test.h"            // DIAG, EXPECT_EQ
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/trace.h"              // TRACE_ARGS

#include <QCoreApplication>

#include <iostream>                    // std::cerr
#include <sstream>                     // std::ostringstream
#include <string>                      // std::string
#include <vector>                      // std::vector


using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// ------------------------- semi-synchronous --------------------------
// First, we demonstrate using the API by using `waitForQtEvent()` to
// wait for IPC.


// If there are no errors in `client`, return "".  Otherwise, return
// them prefixed with a prefix so the whole string is appropriate to
// append to another error message.
std::string getServerErrors(JSON_RPC_Client &lsp)
{
  std::string errMsg = "";
  if (lsp.hasErrorData()) {
    errMsg = stringb("; server error message: " <<
                      lsp.takeErrorData().toStdString());
  }
  return errMsg;
}


// If something has gone wrong with `lsp`, throw `XFormat`.
void checkConnectionStatus(JSON_RPC_Client &lsp)
{
  if (lsp.hasProtocolError()) {
    xformatsb("LSP protocol error: " << lsp.getProtocolError());
  }

  if (!lsp.isChildRunning()) {
    xformatsb("LSP error: server process terminated unexpectedly, no partial message" <<
              getServerErrors(lsp));
  }
}


// Print any pending notifications in `lsp`.
void printNotifications(JSON_RPC_Client &lsp)
{
  while (lsp.hasPendingNotifications()) {
    GDValue notification = lsp.takeNextNotification();
    DIAG("Notification: " << notification.asIndentedString());
  }
}


// Wait for a reply to request `id`, printing any received notifications
// while waiting.
JSON_RPC_Reply printNotificationsUntil(JSON_RPC_Client &lsp, int id)
{
  while (true) {
    checkConnectionStatus(lsp);
    printNotifications(lsp);

    if (lsp.hasReplyForID(id)) {
      return lsp.takeReplyForID(id);
    }

    waitForQtEvent();
  }
}


// Send a request for `method` with `params`.  Synchronously print all
// responses up to and including the reply to that request, which is
// then returned.
JSON_RPC_Reply sendRequestPrintReply(
  JSON_RPC_Client &lsp,
  std::string_view method,
  gdv::GDValue const &params)
{
  int id = lsp.sendRequest(method, params);

  DIAG("Sent request " << method << ", id=" << id << " ...");

  JSON_RPC_Reply reply(printNotificationsUntil(lsp, id));

  DIAG("Reply: " << reply);

  return reply;
}


// As above, but assert that the reply `isSuccess()`.
void sendRequestPrintSuccessReply(
  JSON_RPC_Client &lsp,
  std::string_view method,
  gdv::GDValue const &params)
{
  JSON_RPC_Reply reply(sendRequestPrintReply(lsp, method, params));
  xassert(reply.isSuccess());
}


// Insist the reply is an error, and return the error component.
JSON_RPC_Error sendRequestPrintErrorReply(
  JSON_RPC_Client &lsp,
  std::string_view method,
  gdv::GDValue const &params)
{
  JSON_RPC_Reply reply(sendRequestPrintReply(lsp, method, params));
  xassert(reply.isError());
  return std::move(reply.error());
}


void checkMethodNotFoundError(JSON_RPC_Error const &error)
{
  EXPECT_EQ_GDV(error, fromGDVN(R"(JSON_RPC_Error[
    code: -32601
    message: "The method does not exist (injected error)."
    data: null
  ])"));
}


void checkInvalidRequestError(JSON_RPC_Error const &error)
{
  EXPECT_EQ_GDV(error, fromGDVN(R"(JSON_RPC_Error[
    code: -32600,
    message: "The request is invalid (injected error).",
    data: ["Some", "data", "object", 1, 2, 3]
  ])"));
}


void performLSPInteractionSemiSynchronously(
  JSON_RPC_Client &lsp,
  LSPTestRequestParams const &params)
{
  // Initialize the protocol.
  sendRequestPrintReply(lsp, "initialize", GDVMap{
    // It seems `clangd` ignores this.
    { "processId", GDValue() },

    // This isn't entirely ignored, but it is only used for the
    // "workspace/symbol" request, and even then, only plays a
    // disambiguation role.  Since my intention is to run a single
    // `clangd` server process per machine, it doesn't make sense to
    // initialize it with any particular global "workspace" directory,
    // so I leave this null.
    { "rootUri", GDValue() },

    { "capabilities", GDVMap{} },
  });
  lsp.sendNotification("initialized", GDVMap{});

  // Prepare to ask questions about the source file.
  DIAG("Sending notification textDocument/didOpen ...");
  std::string fnameURI =
    makeFileURI(params.m_fname, URIPathSemantics::NORMAL);
  lsp.sendNotification("textDocument/didOpen", GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", fnameURI },
        { "languageId", "cpp" },
        { "version", 1 },
        { "text", params.m_fileContents },
      }
    }
  });

  // Parameters from the command line that will be passed to each of the
  // next few commands, expressed as GDV.
  GDValue paramsGDV(GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", fnameURI }
      }
    },
    {
      "position",
      GDVMap{
        { "line", params.m_line },
        { "character", params.m_col },
      }
    },
  });

  // Get some info from the LSP server.
  sendRequestPrintSuccessReply(lsp, "textDocument/hover", paramsGDV);
  sendRequestPrintSuccessReply(lsp, "textDocument/declaration", paramsGDV);
  sendRequestPrintSuccessReply(lsp, "textDocument/definition", paramsGDV);
  sendRequestPrintSuccessReply(lsp, "textDocument/completion", paramsGDV);

  {
    JSON_RPC_Error error = sendRequestPrintErrorReply(lsp, "$/methodNotFound", {});
    checkMethodNotFoundError(error);

    error = sendRequestPrintErrorReply(lsp, "$/invalidRequest", {});
    checkInvalidRequestError(error);
  }

  // Shut down the protocol.
  sendRequestPrintReply(lsp, "shutdown", GDVMap{});
  DIAG("Sending notification exit ...");
  lsp.sendNotification("exit", GDVMap{});

  DIAG("Waiting for child to terminate ...");
  while (lsp.isChildRunning()) {
    waitForQtEvent();
  }

  if (lsp.hasErrorData()) {
    DIAG("Server stderr: " << lsp.takeErrorData().toStdString());
  }
}


// ---------------------- asynchronous interface -----------------------
// Now, do the same as above, but using the fully asynchronous
// interface with signals and a top-level event loop.


// The class methods have to be outside the anonymous namespace.
CLOSE_ANONYMOUS_NAMESPACE


JSON_RPC_ClientTester::~JSON_RPC_ClientTester()
{
  DIAG("JSON_RPC_ClientTester dtor");

  QObject::disconnect(&m_lsp, nullptr,
                      this, nullptr);
}


JSON_RPC_ClientTester::JSON_RPC_ClientTester(
  JSON_RPC_Client &lsp,
  LSPTestRequestParams const &params)
  : QObject(),
    m_lsp(lsp),
    m_params(params),
    m_initiatedShutdown(false),
    m_done(false),
    m_failureMsg()
{
  DIAG("JSON_RPC_ClientTester ctor");

  QObject::connect(&m_lsp, &JSON_RPC_Client::signal_hasPendingNotifications,
                   this, &JSON_RPC_ClientTester::on_hasPendingNotifications);
  QObject::connect(&m_lsp, &JSON_RPC_Client::signal_hasReplyForID,
                   this, &JSON_RPC_ClientTester::on_hasReplyForID);
  QObject::connect(&m_lsp, &JSON_RPC_Client::signal_hasProtocolError,
                   this, &JSON_RPC_ClientTester::on_hasProtocolError);
  QObject::connect(&m_lsp, &JSON_RPC_Client::signal_childProcessTerminated,
                   this, &JSON_RPC_ClientTester::on_childProcessTerminated);
}


void JSON_RPC_ClientTester::sendRequestCheckID(
  int expectID,
  std::string_view method,
  GDValue const &params)
{
  int actualID = m_lsp.sendRequest(method, params);
  EXPECT_EQ(actualID, expectID);

  DIAG("Sent request " << method << ", id=" << actualID << " ...");
}


void JSON_RPC_ClientTester::sendNextRequest(
  int prevID, std::optional<JSON_RPC_Reply> const &prevReply)
{
  DIAG("JSON_RPC_Client::sendNextRequest(prevID=" << prevID << ")");

  xassert((prevID==0) == (!prevReply.has_value()));

  std::string fnameURI =
    makeFileURI(m_params.m_fname, URIPathSemantics::NORMAL);

  GDValue params(GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", fnameURI }
      }
    },
    {
      "position",
      GDVMap{
        { "line", m_params.m_line },
        { "character", m_params.m_col },
      }
    },
  });

  // Switch based on the ID we will send next.
  int nextID = prevID+1;
  switch (nextID) {
    case 1:
      // Initialize the protocol.
      sendRequestCheckID(nextID, "initialize", GDVMap{
        { "processId", GDValue() },
        { "rootUri", GDValue() },
        { "capabilities", GDVMap{} },
      });
      break;

    case 2:
      xassert(prevReply->isSuccess());

      // Acknowledge initialization (no reply expected).
      m_lsp.sendNotification("initialized", GDVMap{});

      // Send file contents.
      m_lsp.sendNotification("textDocument/didOpen", GDVMap{
        {
          "textDocument",
          GDVMap{
            { "uri", fnameURI },
            { "languageId", "cpp" },
            { "version", 1 },
            { "text", m_params.m_fileContents },
          }
        }
      });

      // Then immediately ask for hover info.
      sendRequestCheckID(nextID, "textDocument/hover", params);
      break;

    case 3:
      xassert(prevReply->isSuccess());
      sendRequestCheckID(nextID, "textDocument/declaration", params);
      break;

    case 4:
      xassert(prevReply->isSuccess());
      sendRequestCheckID(nextID, "textDocument/definition", params);
      break;

    case 5:
      xassert(prevReply->isSuccess());
      sendRequestCheckID(nextID, "$/methodNotFound", GDVMap{});
      break;

    case 6:
      xassert(prevReply->isError());
      checkMethodNotFoundError(prevReply->right());
      sendRequestCheckID(nextID, "$/invalidRequest", GDVMap{});
      break;

    case 7:
      xassert(prevReply->isError());
      checkInvalidRequestError(prevReply->right());
      sendRequestCheckID(nextID, "shutdown", GDVMap{});
      break;

    case 8:
      // This should cause the child process to exit, which will trigger
      // a signal.
      xassert(prevReply->isSuccess());
      m_lsp.sendNotification("exit", GDVMap{});
      m_initiatedShutdown = true;
      break;

    default:
      xfailure_stringbc("unexpected next ID " << nextID);
      break;
  }
}


void JSON_RPC_ClientTester::setFailureMsg(std::string &&msg)
{
  if (m_failureMsg) {
    // We already have a message, so ignore this one.
  }
  else {
    m_failureMsg = std::move(msg);
    m_done = true;
    DIAG(*m_failureMsg);
  }
}


void JSON_RPC_ClientTester::on_hasPendingNotifications() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("JSON_RPC_ClientTester::on_hasPendingNotifications");

  printNotifications(m_lsp);

  GENERIC_CATCH_END
}


void JSON_RPC_ClientTester::on_hasReplyForID(int id) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("JSON_RPC_ClientTester::on_hasReplyForID(" << id << ")");

  JSON_RPC_Reply reply(m_lsp.takeReplyForID(id));
  DIAG("Reply: " << reply);

  sendNextRequest(id, reply);

  GENERIC_CATCH_END
}


void JSON_RPC_ClientTester::on_hasProtocolError() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("JSON_RPC_ClientTester::on_hasProtocolError");

  setFailureMsg(stringb("Protocol error: " << m_lsp.getProtocolError()));

  GENERIC_CATCH_END
}


void JSON_RPC_ClientTester::on_childProcessTerminated() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("JSON_RPC_ClientTester::on_childProcessTerminated");

  if (m_initiatedShutdown) {
    DIAG("Child process terminated as requested");
    m_done = true;
  }
  else {
    // If there had been a partial message, we would already have a
    // failure message due to what JSON_RPC_Client does internally (and hence
    // the one we make here will be discarded by `setFailureMsg`).
    setFailureMsg(stringb(
      "Child process terminated unexpectedly, no partial message" <<
      getServerErrors(m_lsp)));
  }

  GENERIC_CATCH_END
}


// Re-open the anon namespace for more test functions.
OPEN_ANONYMOUS_NAMESPACE


void performLSPInteractionAsynchronously(
  JSON_RPC_Client &lsp,
  LSPTestRequestParams const &params)
{
  JSON_RPC_ClientTester tester(lsp, params);

  // This kicks off the state machine.  All further steps will be taken
  // in response to specific signals.
  tester.sendNextRequest(0 /*id*/, std::nullopt);

  // Now we just pump the event queue until the state machine says we
  // are done, at which point the child process will have terminated.
  // This loop simulates the application-level event loop used in the
  // real program.
  while (!tester.m_done) {
    waitForQtEvent();
  }

  if (tester.m_failureMsg) {
    // Throw the message as an exception, like in the semi-async case.
    xformat(*(tester.m_failureMsg));
  }
}


// ------------------------------ driver -------------------------------
// Set of possible protocol failures to exercise through deliberate
// injection.  This is meant to reasonably thoroughly exercise the set
// of things that `JSON_RPC_Client::innerProcessOutputData()` checks for, but
// not all conceivable problems.
enum FailureKind {
  // No failure; invoke the real `clangd` and expect normal operation.
  FK_NONE,

  // Server just exits immediately.
  FK_EMPTY_RESPONSE,

  // Server sends back some data, but a header lacks its newline.
  FK_HEADER_LACKS_NEWLINE,

  // There is at least one header line, but no blank line after.
  FK_UNTERMINATED_HEADERS,

  // Headers lack a content length.
  FK_NO_CONTENT_LENGTH,

  // The body data ended before the specified content length.
  FK_INCOMPLETE_BODY,

  // Content-Length is zero.
  FK_ZERO_CONTENT_LENGTH,

  // Content-Length does not parse as an integer.
  FK_INVALID_CONTENT_LENGTH,

  // Body JSON is malformed.
  FK_MALFORMED_JSON,

  // The body JSON is not a JSON map (object).
  FK_NOT_A_MAP,

  // The response has an invalid `id` member.
  FK_INVALID_ID,

  // The response has a negative `id` member.
  FK_NEGATIVE_ID,

  NUM_FAILURE_KINDS
};

// Iterate over all of the FailureKinds.
#define FOREACH_FAILURE_KIND(fkvar)                     \
  for (FailureKind fkvar = static_cast<FailureKind>(0); \
       fkvar != NUM_FAILURE_KINDS;                      \
       fkvar = static_cast<FailureKind>(fkvar+1))


// Return a string that should appear as a substring of the protocol
// error message triggered by an LSP server behaving according to `fk`.
char const *substringForFK(FailureKind fk)
{
  RETURN_ENUMERATION_STRING_OR(
    FailureKind,
    NUM_FAILURE_KINDS,
    (
      "(no failure)",
      "terminated unexpectedly, no partial",
      "lacked a terminating newline",
      "did not end with a blank line",
      "No Content-Length",
      "body ended before the specified Content-Length",
      "Content-Length value was zero",
      "Invalid character",
      "while looking for a value after '['",
      "Expected map",
      "Expected small integer",
      "ID is negative: -1",
    ),
    fk,
    "(invalid FailureKind)"
  )
}


// Return the response that we want the server process to produce in
// order to exercise `fk`.
char const *responseForFK(FailureKind fk)
{
  RETURN_ENUMERATION_STRING_OR(
    FailureKind,
    NUM_FAILURE_KINDS,
    (
      "(no failure; not used)",
      "",
      "some-junk",
      R"(misc-header: foo\n)",
      R"(other-header: foo\n\nblah\n)",
      R"(Content-Length: 999\n\n[]\n)",
      R"(Content-Length: 0\n\n)",
      R"(Content-Length: nonsense\n)",
      R"(Content-Length: 2\n\n[\n)",
      R"(Content-Length: 3\n\n[]\n)",
      R"(Content-Length: 13\n\n{"id":"junk"}\n)",
      R"(Content-Length: 9\n\n{"id":-1}\n)",
    ),
    fk,
    "(invalid FailureKind)"
  )
}


void runTests(
  bool semiSynchronous,
  FailureKind failureKind,
  LSPTestRequestParams const &params)
{
  // Prepare to start the server process.
  CommandRunner cr;
  if (failureKind == FK_NONE) {
    // Use a server that behaves properly (protocol-wise).
    if (params.m_useRealClangd) {
      // Use the real `clangd`.  This is only for interactive use, not
      // automated tests.
      cr.setProgram("clangd");
    }
    else {
      // Use a mock server that just does basic protocol stuff.  This is
      // much faster and has fewer dependencies, so is good for
      // automated testing.
      //
      // We have to use `env` here rather than invoking `python3`
      // directly because, under Cygwin, the latter is a symlink and
      // consequently Windows `CreateProcess` cannot start it.
      //
      cr.setProgram("env");
      cr.setArguments(QStringList() << "python3" << "./lsp-test-server.py");
    }
  }
  else /*failureKind != FK_NONE*/ {
    char const *badResponse = responseForFK(failureKind);

    // Use `printf` rather than `echo` for better portability.
    cr.setProgram("printf");
    cr.setArguments(QStringList() << toQString(badResponse));
  }

  // Actually start the server.
  cr.startAsynchronous();
  if (!cr.waitForStarted(5000 /*ms*/)) {
    std::cout << "Failed to start server process: "
              << cr.getErrorMessage() << "\n";
    xfailure("server failed");
  }

  std::ostringstream diagnosticLog;
  try {
    // Wrap it in the JSON-RPC client protocol object.
    JSON_RPC_Client lsp(cr, &diagnosticLog);

    // Do all the protocol stuff.
    if (semiSynchronous) {
      performLSPInteractionSemiSynchronously(lsp, params);
    }
    else {
      performLSPInteractionAsynchronously(lsp, params);
    }
  }
  catch (XBase &x) {
    cr.killProcess();

    // This is just for manual inspection.
    if (std::string logMsg = diagnosticLog.str(); !logMsg.empty()) {
      DIAG("diagnostic log: " << logMsg);
    }

    if (failureKind != FK_NONE) {
      std::string msg = x.what();
      std::string expectSubstring = substringForFK(failureKind);
      if (hasSubstring(msg, expectSubstring)) {
        DIAG("As expected: " << msg);
      }
      else {
        std::cout << "Got failure msg: " << doubleQuote(msg) << "\n"
                  << "but expected substring " << doubleQuote(expectSubstring) << "\n"
                  << "was missing.\n";
        xfailure("wrong exception message");
      }
      return;
    }
    else {
      // Exception in the non-failure case, re-throw it.
      throw;
    }
  }

  // The failure cases should not get here.
  xassert(failureKind == FK_NONE);

  // For the non-failure case, we expect everything to look ok at the
  // end.
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getExitCode(), 0);
}


CLOSE_ANONYMOUS_NAMESPACE


void test_json_rpc_client(CmdlineArgsSpan args)
{
  LSPTestRequestParams params =
    LSPTestRequestParams::getFromCmdLine(args);

  std::set<bool> booleans = {false, true};
  for (bool async : booleans) {
    char const *syncLabel = (async? "asynchronous" : "semi-synchronous");
    FOREACH_FAILURE_KIND(fkind) {
      DIAG("------------ " << syncLabel << ", fkind=" << fkind << " -----------");
      runTests(!async, fkind, params);
    }
  }
}


// EOF
