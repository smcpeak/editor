// lsp-client-manager-test.cc
// Tests for `lsp-client-manager` module.

#include "unit-tests.h"                // decl for my entry point
#include "lsp-client-manager.h"        // module under test

#include "doc-type-detect.h"           // detectDocumentType
#include "json-rpc-reply.h"            // JSON_RPC_Reply
#include "lsp-client.h"                // normalizeLSPPath
#include "lsp-conv.h"                  // lspLanguageIdForDT
#include "named-td-list.h"             // NamedTextDocumentList
#include "uri-util.h"                  // makeFileURI
#include "vfs-test-connections.h"      // VFS_TestConnections

#include "smqtutil/qtutil.h"           // waitForQtEvent

#include "smbase/gdvalue.h"            // gdv::fromGDVN
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, EXPECT_{TRUE,FALSE}

#include <functional>                  // std::function

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void waitUntil(char const *desc, std::function<bool()> condition)
{
  DIAG("Waiting until: " << desc);
  while (!condition()) {
    waitForQtEvent();
  }
  DIAG("Finished waiting until: " << desc);
}


/* Run through a simple "happy path" lifecycle:

     - Start the server.
     - Open a document.
     - Make a couple requests.
     - Close the document.
     - Stop the server.
*/
void test_basics()
{
  NamedTextDocumentList documentList;
  VFS_TestConnections vfsConnections;

  LSPClientManager lcm(
    &documentList,
    &vfsConnections,
    false /*useRealServer*/,
    "out/log" /*logFileDirectory*/,
    &std::cerr /*protocolDiagnosticLog*/);

  EXPECT_FALSE(lcm.useRealServer());

  // Make a document.
  NamedTextDocument *ntd1 = new NamedTextDocument;
  ntd1->setDocumentName(DocumentName::fromLocalFilename(
    normalizeLSPPath("foo.cc")));
  char const *ntd1Contents = "one\ntwo\n";
  ntd1->replaceWholeFileString(ntd1Contents);
  ntd1->setDocumentType(detectDocumentType(ntd1->documentName()));
  documentList.addDocument(ntd1);      // Ownership transfer.
  EXPECT_TRUE(ntd1->isCompatibleWithLSP());
  EXPECT_FALSE(ntd1->getNumDiagnostics().has_value());

  // Currently, no `LSPClient` exists.
  xassert(lcm.getClientOptC(ntd1) == nullptr);
  xassert(lcm.getClientOpt(ntd1) == nullptr);
  EXPECT_EQ(lcm.getProtocolState(ntd1), LSP_PS_CLIENT_INACTIVE);
  EXPECT_FALSE(lcm.isRunningNormally(ntd1));
  EXPECT_FALSE(lcm.isInitializing(ntd1));
  EXPECT_EQ(lcm.explainAbnormality(ntd1),
    "The LSP server has not been started.");
  EXPECT_HAS_SUBSTRING(lcm.getServerStatus(ntd1),
    "There is no LSP client object for this document's scope.");

  // Make one.
  RCSerfOpt<LSPClient> client = lcm.getOrCreateClient(ntd1);
  xassert(client != nullptr);

  // Now it can be looked up.
  xassert(lcm.getClientOptC(ntd1) == client);
  xassert(lcm.getClientOpt(ntd1) == client);

  // But it is still not running.
  EXPECT_EQ(lcm.getProtocolState(ntd1), LSP_PS_CLIENT_INACTIVE);
  EXPECT_FALSE(lcm.isRunningNormally(ntd1));
  EXPECT_FALSE(lcm.isInitializing(ntd1));
  EXPECT_HAS_SUBSTRING(lcm.explainAbnormality(ntd1),
    "The LSP server has not been started.");
  EXPECT_HAS_SUBSTRING(lcm.getServerStatus(ntd1),
    "The LSP server has not been started.");

  // Check its log file location.
  EXPECT_EQ(client->lspStderrLogFname().value(),
    "out/log/lsp-server-local-cpp.log");

  // Start it.
  EXPECT_FALSE(lcm.startServer(ntd1).has_value());

  // At first, we are initializing.
  EXPECT_EQ(lcm.getProtocolState(ntd1), LSP_PS_INITIALIZING);
  EXPECT_FALSE(lcm.isRunningNormally(ntd1));
  EXPECT_TRUE(lcm.isInitializing(ntd1));
  EXPECT_HAS_SUBSTRING(lcm.explainAbnormality(ntd1),
    "The \"initialize\" request has been sent");
  EXPECT_HAS_SUBSTRING(lcm.getServerStatus(ntd1),
    "The \"initialize\" request has been sent");

  // Wait for it to initialize.
  waitUntil("LSP finished initializing", [&lcm, ntd1]() -> bool {
    return !lcm.isInitializing(ntd1);
  });
  EXPECT_EQ(lcm.getProtocolState(ntd1), LSP_PS_NORMAL);

  EXPECT_TRUE(lcm.isRunningNormally(ntd1));
  EXPECT_FALSE(lcm.isInitializing(ntd1));
  EXPECT_HAS_SUBSTRING(lcm.explainAbnormality(ntd1),
    "The LSP server is running normally.");
  EXPECT_HAS_SUBSTRING(lcm.getServerStatus(ntd1),
    "The LSP server is running normally.");

  EXPECT_TRUE(client->isRunningNormally());

  // File is not open yet.
  EXPECT_FALSE(lcm.fileIsOpen(ntd1));
  xassert(lcm.getDocInfo(ntd1) == nullptr);

  // Open the file.
  ntd1->beginTrackingChanges();
  lcm.openFile(ntd1, lspLanguageIdForDT(ntd1->documentType()));
  lcm.selfCheck();

  // Check that it appears as open now.
  EXPECT_TRUE(lcm.fileIsOpen(ntd1));
  RCSerfOpt<LSPDocumentInfo const> docInfo = lcm.getDocInfo(ntd1);
  xassert(docInfo != nullptr);

  EXPECT_TRUE(client->isFileOpen(ntd1->filename()));

  // Wait for the initial diagnostics.  When they arrive,
  // `LSPClientManager` should take care of attaching them to `ntd1`.
  waitUntil("initial diagnostics arrived", [ntd1]() -> bool {
    return ntd1->getNumDiagnostics().has_value();
  });
  EXPECT_EQ(ntd1->getNumDiagnostics().value(), 0);

  // Request the uses of the first symbol.  This request is issued via
  // `LSPClient` because `LSPClientManager` does not expose it.
  {
    int requestID = client->requestRelatedLocation(
      LSPSymbolRequestKind::K_REFERENCES,
      ntd1->filename(),
      TextMCoord(LineIndex(0), ByteIndex(0)));

    // Wait for the reply using `LSPClientManager`.
    waitUntil("References reply arrived",
              [&lcm, ntd1, requestID]() -> bool {
      return lcm.hasReplyForID(ntd1, requestID);
    });

    JSON_RPC_Reply reply = lcm.takeReplyForID(ntd1, requestID);
    EXPECT_TRUE(reply.isSuccess());
    VPVAL(reply.result());
  }

  lcm.selfCheck();

  // Make a request through `LSPClientManager`.
  {
    int requestID = lcm.sendArbitraryRequest(
      ntd1,
      "$/getTextDocumentContents",
      GDValue(GDVMap{
        { "textDocument", GDVMap{
           { "uri", makeFileURI(ntd1->filename()) }
        }}
      }));

    waitUntil("getTextDocumentContents reply arrived",
              [&lcm, ntd1, requestID]() -> bool {
      return lcm.hasReplyForID(ntd1, requestID);
    });

    JSON_RPC_Reply reply = lcm.takeReplyForID(ntd1, requestID);
    EXPECT_TRUE(reply.isSuccess());

    VPVAL(reply.result());
    EXPECT_EQ(reply.result().mapGetValueAt("text").stringGet(),
              ntd1Contents);

  }

  lcm.selfCheck();

  // Reset this since the object it points to will go away.
  docInfo = nullptr;

  // Close the file.
  lcm.closeFile(ntd1);
  lcm.selfCheck();

  EXPECT_FALSE(lcm.fileIsOpen(ntd1));
  EXPECT_FALSE(client->isFileOpen(ntd1->filename()));

  // Reset this too, as it is about to go away.
  client = nullptr;

  // Shut down the server.
  lcm.stopServer(ntd1);

  EXPECT_EQ(lcm.getProtocolState(ntd1), LSP_PS_SHUTDOWN1);
  EXPECT_FALSE(lcm.isRunningNormally(ntd1));

  // Wait for it to finish shutting down.
  waitUntil("LSP server has shut down",
            [&lcm, ntd1]() -> bool {
    return lcm.getProtocolState(ntd1) == LSP_PS_CLIENT_INACTIVE;
  });

  EXPECT_FALSE(lcm.isRunningNormally(ntd1));
  EXPECT_HAS_SUBSTRING(lcm.explainAbnormality(ntd1),
    "The LSP server has not been started.");
  EXPECT_HAS_SUBSTRING(lcm.getServerStatus(ntd1),
    "The LSP server has not been started.");
}


void test_LSPClientScope_description()
{
  EXPECT_EQ(
    LSPClientScope(HostName::asLocal(),
                   DocumentType::DT_CPP).description(),
    "C++ files on local host");

  EXPECT_EQ(
    LSPClientScope(HostName::asSSH("some-machine"),
                   DocumentType::DT_OCAML).description(),
    "OCaml files on ssh:some-machine host");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_lsp_client_manager(CmdlineArgsSpan args)
{
  test_basics();
  test_LSPClientScope_description();
}


// EOF
