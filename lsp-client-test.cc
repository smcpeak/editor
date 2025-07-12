// lsp-client-test.cc
// Tests for `lsp-client`.

#include "lsp-client.h"                // module under test

#include "command-runner.h"            // CommandRunner

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


void performLSPInteraction(
  LSPClient &lsp,
  std::string const &fname,
  int line,
  int col,
  std::string const &fileContents)
{
  SMFileUtil sfu;

  // Initialize the protocol.
  lsp.sendRequestPrintReply("initialize", GDVMap{
    { "rootUri", LSPClient::makeFileURI(sfu.currentDirectory()) },
    { "capabilities", GDVMap{} },
  });
  lsp.sendNotification("initialized", GDVMap{});

  // Prepare to ask questions about the source file.
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
  lsp.sendRequestPrintReply("textDocument/hover", params);
  lsp.sendRequestPrintReply("textDocument/declaration", params);
  lsp.sendRequestPrintReply("textDocument/definition", params);

  // Shut down the protocol.
  lsp.sendRequestPrintReply("shutdown", GDVMap{});
  lsp.sendNotification("exit", GDVMap{});
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

  // Start `clangd`.
  CommandRunner cr;
  cr.setProgram("clangd");
  cr.startAsynchronous();
  EXPECT_EQ(cr.waitForStarted(5000 /*ms*/), true);

  {
    // Wrap it in the LSP client object.
    LSPClient lsp(cr);

    // Do all the protocol stuff.
    performLSPInteraction(lsp, fname, line, col, fileContents);
  }

  cr.waitForNotRunning();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getExitCode(), 0);
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
