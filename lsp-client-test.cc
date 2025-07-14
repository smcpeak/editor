// lsp-client-test.cc
// Tests for `lsp-client`.

#include "lsp-client-test.h"           // decls for this test module

#include "lsp-client.h"                // module under test

#include "command-runner.h"            // CommandRunner

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // ARGS_MAIN, VPVAL
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/trace.h"              // TRACE_ARGS, EXPECT_EQ

#include <QCoreApplication>

#include <cstdlib>                     // std::exit
#include <iostream>                    // std::cerr


using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


// ------------------------- semi-synchronous --------------------------
// First, we demonstrate using the API by using `waitForQtEvent()` to
// wait for IPC.

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
    m_done(false),
    m_failed(false)
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
      break;

    default:
      xfailure_stringbc("unexpected next ID " << nextID);
      break;
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

  std::cout << "Protocol error: " << m_lsp.getProtocolError() << "\n";
  m_done = true;
  m_failed = true;

  GENERIC_CATCH_END
}


void LSPClientTester::on_childProcessTerminated() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  DIAG("LSPClientTester::on_childProcessTerminated");

  std::cout << "Child process terminated\n";
  m_done = true;

  GENERIC_CATCH_END
}


// Re-open the anon namespace for more test functions.
OPEN_ANONYMOUS_NAMESPACE


void performLSPInteractionSynchronously(
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

  // Now we just pump the event queue until the machine says we are
  // done, at which point the child process will have terminated.  This
  // loop simulates the application-level event loop used in the real
  // program.
  while (!tester.m_done) {
    waitForQtEvent();
  }

  xassert(!tester.m_failed);
}


// ------------------------------ driver -------------------------------
void runTests(
  bool semiSynchronous,
  std::string const &fname,
  int line,
  int col,
  std::string const &fileContents)
{
  // Start `clangd`.
  CommandRunner cr;
  cr.setProgram("clangd");
  cr.startAsynchronous();
  EXPECT_EQ(cr.waitForStarted(5000 /*ms*/), true);

  {
    // Wrap it in the LSP client object.
    LSPClient lsp(cr);

    // Do all the protocol stuff.
    if (semiSynchronous) {
      performLSPInteractionSemiSynchronously(
        lsp, fname, line, col, fileContents);
    }
    else {
      performLSPInteractionSynchronously(
        lsp, fname, line, col, fileContents);
    }
  }

  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getExitCode(), 0);
}


void entry(int argc, char **argv)
{
  TRACE_ARGS();

  // Interpret command line.
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " <file> <line> <col>\n";
    std::exit(2);
  }
  std::string fname(argv[1]);
  int line = std::atoi(argv[2]);
  int col = std::atoi(argv[3]);

  // Read the source file of interest.
  SMFileUtil sfu;
  std::string fileContents = sfu.readFileAsString(fname);

  // Enable Qt event loop, etc.
  QCoreApplication app(argc, argv);

  std::cout << "------------ semi-synchonous -----------\n";
  runTests(true /*semiSync*/, fname, line, col, fileContents);

  std::cout << "------------ asynchonous -----------\n";
  runTests(false /*semiSync*/, fname, line, col, fileContents);
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
