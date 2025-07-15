// lsp-client-test.cc
// Tests for `lsp-client`.

#include "lsp-client-test.h"           // decls for this test module

#include "lsp-client.h"                // module under test

#include "command-runner.h"            // CommandRunner

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/exc.h"                // smbase::XBase
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE, RETURN_ENUMERATION_STRING_OR
#include "smbase/sm-test.h"            // ARGS_MAIN, VPVAL
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/trace.h"              // TRACE_ARGS, EXPECT_EQ

#include <QCoreApplication>

#include <cstdlib>                     // std::exit
#include <iostream>                    // std::cerr


using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// ------------------------- semi-synchronous --------------------------
// First, we demonstrate using the API by using `waitForQtEvent()` to
// wait for IPC.


// If there are no errors in `lsp`, return "".  Otherwise, return them
// prefixed with a prefix so the whole string is appropriate to append
// to another error message.
std::string getServerErrors(LSPClient &lsp)
{
  std::string errMsg = "";
  if (lsp.hasErrorData()) {
    errMsg = stringb("; server error message: " <<
                      lsp.takeErrorData().toStdString());
  }
  return errMsg;
}


// If something has gone wrong with `lsp`, throw `XFormat`.
void checkConnectionStatus(LSPClient &lsp)
{
  if (lsp.hasProtocolError()) {
    xformatsb("LSP protocol error: " << lsp.getProtocolError());
  }

  if (!lsp.isChildRunning()) {
    xformatsb("LSP error: server process terminated unexpectedly" <<
              getServerErrors(lsp));
  }
}


// Print any pending notifications in `lsp.
void printNotifications(LSPClient &lsp)
{
  while (lsp.hasPendingNotifications()) {
    GDValue notification = lsp.takeNextNotification();
    std::cout << "Notification: " << notification.asLinesString();
  }
}


// Wait for a reply to request `id`, printing any received notifications
// while waiting.
GDValue printNotificationsUntil(LSPClient &lsp, int id)
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
// responses up to and including the reply to that request.
void sendRequestPrintReply(
  LSPClient &lsp,
  std::string_view method,
  gdv::GDValue const &params)
{
  int id = lsp.sendRequest(method, params);

  std::cout << "Sent request " << method << ", id=" << id << " ...\n";

  GDValue resp(printNotificationsUntil(lsp, id));

  std::cout << "Response: " << resp.asLinesString();
}


void performLSPInteractionSemiSynchronously(
  LSPClient &lsp,
  std::string const &fname,
  int line,
  int col,
  std::string const &fileContents)
{
  SMFileUtil sfu;

  // Initialize the protocol.
  sendRequestPrintReply(lsp, "initialize", GDVMap{
    { "rootUri", LSPClient::makeFileURI(sfu.currentDirectory()) },
    { "capabilities", GDVMap{} },
  });
  lsp.sendNotification("initialized", GDVMap{});

  // Prepare to ask questions about the source file.
  std::cout << "Sending notification textDocument/didOpen ...\n";
  std::string fnameURI = LSPClient::makeFileURI(fname);
  lsp.sendNotification("textDocument/didOpen", GDVMap{
    {
      "textDocument",
      GDVMap{
        { "uri", fnameURI },
        { "languageId", "cpp" },
        { "version", 1 },
        { "text", fileContents },
      }
    }
  });

  // Parameters from the command line that will be passed to each of the
  // next few commands.
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
        { "line", line },
        { "character", col },
      }
    },
  });

  // Get some info from the LSP server.
  sendRequestPrintReply(lsp, "textDocument/hover", params);
  sendRequestPrintReply(lsp, "textDocument/declaration", params);
  sendRequestPrintReply(lsp, "textDocument/definition", params);

  // Shut down the protocol.
  sendRequestPrintReply(lsp, "shutdown", GDVMap{});
  std::cout << "Sending notification exit ...\n";
  lsp.sendNotification("exit", GDVMap{});

  std::cout << "Waiting for child to terminate ...\n";
  while (lsp.isChildRunning()) {
    waitForQtEvent();
  }
}


// ---------------------- asynchronous interface -----------------------
// Now, do the same as above, but using the fully asynchronous
// interface with signals and a top-level event loop.


// The class methods have to be outside the anonymous namespace.
CLOSE_ANONYMOUS_NAMESPACE


LSPClientTester::~LSPClientTester()
{
  DIAG("LSPClientTester dtor");

  QObject::disconnect(&m_lsp, nullptr,
                      this, nullptr);
}


LSPClientTester::LSPClientTester(
  LSPClient &lsp,
  std::string const &fname,
  int line,
  int col,
  std::string const &fileContents)
  : QObject(),
    m_lsp(lsp),
    m_fname(fname),
    m_line(line),
    m_col(col),
    m_fileContents(fileContents),
    m_initiatedShutdown(false),
    m_done(false),
    m_failureMsg()
{
  DIAG("LSPClientTester ctor");

  QObject::connect(&m_lsp, &LSPClient::signal_hasPendingNotifications,
                   this, &LSPClientTester::on_hasPendingNotifications);
  QObject::connect(&m_lsp, &LSPClient::signal_hasReplyForID,
                   this, &LSPClientTester::on_hasReplyForID);
  QObject::connect(&m_lsp, &LSPClient::signal_hasProtocolError,
                   this, &LSPClientTester::on_hasProtocolError);
  QObject::connect(&m_lsp, &LSPClient::signal_childProcessTerminated,
                   this, &LSPClientTester::on_childProcessTerminated);
}


void LSPClientTester::sendRequestCheckID(
  int expectID,
  std::string_view method,
  GDValue const &params)
{
  int actualID = m_lsp.sendRequest(method, params);
  EXPECT_EQ(actualID, expectID);

  std::cout << "Sent request " << method << ", id=" << actualID << " ...\n";
}


void LSPClientTester::sendNextRequest(int prevID)
{
  DIAG("LSPClient::sendNextRequest(prevID=" << prevID << ")");

  SMFileUtil sfu;

  std::string fnameURI = LSPClient::makeFileURI(m_fname);

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
        { "line", m_line },
        { "character", m_col },
      }
    },
  });

  // Switch based on the ID we will send next.
  int nextID = prevID+1;
  switch (nextID) {
    case 1:
      // Initialize the protocol.
      sendRequestCheckID(nextID, "initialize", GDVMap{
        { "rootUri", LSPClient::makeFileURI(sfu.currentDirectory()) },
        { "capabilities", GDVMap{} },
      });
      break;

    case 2:
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
            { "text", m_fileContents },
          }
        }
      });

      // Then immediately ask for hover info.
      sendRequestCheckID(nextID, "textDocument/hover", params);
      break;

    case 3:
      sendRequestCheckID(nextID, "textDocument/declaration", params);
      break;

    case 4:
      sendRequestCheckID(nextID, "textDocument/definition", params);
      break;

    case 5:
      sendRequestCheckID(nextID, "shutdown", GDVMap{});
      break;

    case 6:
      // This should cause the child process to exit, which will trigger
      // a signal.
      m_lsp.sendNotification("exit", GDVMap{});
      m_initiatedShutdown = true;
      break;

    default:
      xfailure_stringbc("unexpected next ID " << nextID);
      break;
  }
}


void LSPClientTester::setFailureMsg(std::string &&msg)
{
  if (m_failureMsg) {
    // We already have a message, so ignore this one.
  }
  else {
    m_failureMsg = std::move(msg);
    m_done = true;
    std::cout << *m_failureMsg << "\n";
  }
}


void LSPClientTester::on_hasPendingNotifications() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPClientTester::on_hasPendingNotifications");

  printNotifications(m_lsp);

  GENERIC_CATCH_END
}


void LSPClientTester::on_hasReplyForID(int id) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPClientTester::on_hasReplyForID(" << id << ")");

  GDValue resp(m_lsp.takeReplyForID(id));
  std::cout << "Response: " << resp.asLinesString();

  sendNextRequest(id);

  GENERIC_CATCH_END
}


void LSPClientTester::on_hasProtocolError() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPClientTester::on_hasProtocolError");

  setFailureMsg(stringb("Protocol error: " << m_lsp.getProtocolError()));

  GENERIC_CATCH_END
}


void LSPClientTester::on_childProcessTerminated() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPClientTester::on_childProcessTerminated");

  if (m_initiatedShutdown) {
    std::cout << "Child process terminated as requested\n";
    m_done = true;
  }
  else {
    setFailureMsg(stringb("Child process terminated unexpectedly" <<
                          getServerErrors(m_lsp)));
  }

  GENERIC_CATCH_END
}


// Re-open the anon namespace for more test functions.
OPEN_ANONYMOUS_NAMESPACE


void performLSPInteractionAsynchronously(
  LSPClient &lsp,
  std::string const &fname,
  int line,
  int col,
  std::string const &fileContents)
{
  LSPClientTester tester(lsp, fname, line, col, fileContents);

  // This kicks off the state machine.  All further steps will be taken
  // in response to specific signals.
  tester.sendNextRequest(0 /*id*/);

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
// injection.
enum FailureKind {
  // No failure; invoke the real `clangd` and expect normal operation.
  FK_NONE,

  // Server just exits immediately.
  FK_EMPTY_RESPONSE,

  // Server sends back some data, but not forming a complete set of
  // headers.
  FK_INCOMPLETE_HEADERS,

  // Headers lack a content length.
  FK_NO_CONTENT_LENGTH,

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
      "terminated unexpectedly",
      "incomplete message",
      "No Content-Length",
    ),
    fk,
    "(invalid FailureKind)"
  )
}


void runTests(
  bool useRealClangd,
  bool semiSynchronous,
  FailureKind failureKind,
  std::string const &fname,
  int line,
  int col,
  std::string const &fileContents)
{
  // Start the server process.
  CommandRunner cr;
  switch (failureKind) {
    default:
      xfailure("bad FailureKind");

    case FK_NONE:
      // Use a proper server to test proper protocol interaction.
      if (useRealClangd) {
        // Use the real `clangd`.
        cr.setProgram("clangd");
      }
      else {
        // Use a mock server that just does basic protocol stuff.  This
        // is much faster and has fewer dependencies, so is good for
        // routine automated testing.
        //
        // We have to use `env` here rather than invoking `python3`
        // directly because the latter is a cygwin symlink and
        // consequently Windows `CreateProcess` cannot start it.
        cr.setProgram("env");
        cr.setArguments(QStringList() << "python3" << "./lsp-test-server.py");
      }
      break;

    case FK_EMPTY_RESPONSE:
      cr.setProgram("true");
      break;

    case FK_INCOMPLETE_HEADERS:
      cr.setProgram("printf");
      cr.setArguments(QStringList() << "some-junk");
      break;

    case FK_NO_CONTENT_LENGTH:
      cr.setProgram("printf");
      cr.setArguments(QStringList() << R"(other-header: foo\n\nblah\n)");
      break;
  }
  cr.startAsynchronous();
  if (!cr.waitForStarted(5000 /*ms*/)) {
    std::cout << "Failed to start server process: "
              << cr.getErrorMessage() << "\n";
    xfailure("server failed");
  }

  try {
    // Wrap it in the LSP client object.
    LSPClient lsp(cr);

    // Do all the protocol stuff.
    if (semiSynchronous) {
      performLSPInteractionSemiSynchronously(
        lsp, fname, line, col, fileContents);
    }
    else {
      performLSPInteractionAsynchronously(
        lsp, fname, line, col, fileContents);
    }
  }
  catch (XBase &x) {
    cr.killProcess();
    if (failureKind != FK_NONE) {
      std::string msg = x.what();
      std::string expectSubstring = substringForFK(failureKind);
      if (hasSubstring(msg, expectSubstring)) {
        std::cout << "As expected: " << msg << "\n";
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


void entry(int argc, char **argv)
{
  TRACE_ARGS();

  // Default query parameters, used when run without arguments.
  std::string fname = "eclf.h";
  int line = 9;
  int col = 5;
  bool useRealClangd = false;

  // Interpret command line.
  if (argc != 1) {
    if (argc != 4) {
      std::cerr << "Usage: " << argv[0] << " <file> <line> <col>\n";
      std::exit(2);
    }
    fname = argv[1];
    line = std::atoi(argv[2]);
    col = std::atoi(argv[3]);
    useRealClangd = true;
  }

  // Read the source file of interest.
  SMFileUtil sfu;
  std::string fileContents = sfu.readFileAsString(fname);

  // Enable Qt event loop, etc.
  QCoreApplication app(argc, argv);

  std::set<bool> booleans = {false, true};
  for (bool async : booleans) {
    char const *syncLabel = (async? "asynchronous" : "semi-synchronous");
    FOREACH_FAILURE_KIND(fkind) {
      std::cout << "------------ " << syncLabel << ", fkind=" << fkind << " -----------\n";
      runTests(useRealClangd, !async, fkind,
               fname, line, col, fileContents);
    }
  }
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
