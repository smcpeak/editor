// lsp-get-code-lines.cc
// Code for `lsp-get-code-lines` module.

#include "lsp-get-code-lines.h"        // this module

#include "td-core.h"                   // TextDocumentCore
#include "host-file-and-line-opt.h"    // HostFileAndLineOpt
#include "lsp-manager.h"               // LSPManagerDocumentState
#include "vfs-connections.h"           // VFS_AbstractConnections
#include "vfs-query-sync.h"            // readFileSynchronously

#include "smqtutil/sync-wait.h"        // SynchronousWaiter

#include "smbase/either.h"             // smbase::Either
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-set.h"        // gdv::toGDValue(std::set)
#include "smbase/gdvalue-vector.h"     // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/map-util.h"           // smbase::mapInsertUniqueMove
#include "smbase/sm-env.h"             // smbase::envAsIntOr
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

#include <memory>                      // std::make_unique
#include <optional>                    // std::optional
#include <string>                      // std::string
#include <vector>                      // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-get-code-lines");


/* It might seem excessive to factor this one function, and each of
   the interfaces it uses, just so it can be tested in isolation.  The
   code is not that long after all, just a little over 100 lines.

   However, it sits at the nexus of three different IPC mechanisms:

     1. The user interface and the user's ability to cancel waits,
        embodied by `waiter`.

     2. Communication with the LSP server, embodied by `lspManager`.
        (This function does not actually perform any LSP communication,
        hence the `const` qualifier, but it accesses data closely
        related to it.)

     3. Communication with the VFS server(s), embodied by
        `vfsConnections`.

   Since each of these can be in various states on entry, and the
   several communication attempts can have various outcomes, it seems
   worthwhile to engineer this function for separate testability.  It
   might also serve as an example for similar efforts elsewhere.
*/
std::optional<std::vector<std::string>> lspGetCodeLinesFunction(
  SynchronousWaiter &waiter,
  std::vector<HostFileAndLineOpt> const &locations,
  LSPManagerDocumentState const &lspManager,
  VFS_AbstractConnections &vfsConnections)
{
  TRACE2_GDVN_EXPRS("lspGetCodeLines", locations);

  // First, get the set of files that require a VFS query.
  std::set<HostAndResourceName> filesToQuery;
  for (HostFileAndLineOpt const &hfal : locations) {
    xassertPrecondition(hfal.hasFilename() && hfal.hasLine());

    HostAndResourceName const &harn = hfal.getHarn();
    if (harn.isLocal() &&
        !lspManager.isFileOpen(harn.resourceName())) {
      // It is a local file, but it is not open with the LSP manager, so
      // we will need to query for it.
      filesToQuery.insert(harn);
    }
  }
  TRACE2_GDVN_EXPRS("lspGetCodeLines", filesToQuery);

  // Either a parsed (into lines of text) document, or an error message
  // explaining why the contents could not be read.
  using DocOrError =
    Either<std::unique_ptr<TextDocumentCore>, std::string>;

  // Map holding the result of the queries.
  std::map<HostAndResourceName, DocOrError> nameToDocOrError;

  // Issue a query for each of those files.
  for (HostAndResourceName const &harn : filesToQuery) {
    // Issue a request and synchronously wait for the reply.
    //
    // TODO: This queries one file at a time.  I want to batch all of
    // the requests into a single message.
    TRACE2("lspGetCodeLines: querying: " << toGDValue(harn));
    auto replyOrError(
      readFileSynchronously(&vfsConnections, waiter, harn));

    if (std::optional<std::string> errorMsg =
          getROEErrorMessage(replyOrError)) {
      // Store the error message.
      TRACE2("lspGetCodeLines: got error for " << toGDValue(harn) <<
             ": " << *errorMsg);
      mapInsertUniqueMove(nameToDocOrError, harn,
        DocOrError(*errorMsg));
    }
    else {
      std::unique_ptr<VFS_ReadFileReply> &rfr = replyOrError.left();
      if (rfr) {
        // Error would be handled above.
        xassert(rfr->m_success);

        // Copy the contents into a `TextDocumentCore` for easy line
        // querying later.
        auto doc = std::make_unique<TextDocumentCore>();
        doc->replaceWholeFile(rfr->m_contents);

        // Store the contents in the map.
        mapInsertUniqueMove(nameToDocOrError, harn,
          DocOrError(std::move(doc)));
      }
      else {
        // User canceled the wait, bail out entirely.
        TRACE2("lspGetCodeLines: while querying " << toGDValue(harn) <<
               ", user canceled");
        return std::nullopt;
      }
    }
  }

  // TODO: Provide GDV serialization for Either, and use it here to
  // trace the value of `nameToDocOrError`.

  // Allow injecting an offset to test handling of invalid (too large)
  // line indices.
  static LineCount const offsetForTesting(
    envAsIntOr(0, "EDITOR_GLOBAL_GET_CODE_LINE_OFFSET"));

  // Now go over the original set of locations again, populating the
  // sequence to return.
  std::vector<std::string> ret;
  for (HostFileAndLineOpt const &hfal : locations) {
    // Already checked above, but no harm in checking again.
    xassertPrecondition(hfal.hasFilename() && hfal.hasLine());

    HostAndResourceName const &harn = hfal.getHarn();
    if (!harn.isLocal()) {
      ret.push_back(stringb("<Not local: " << harn << ">"));
    }

    std::string const fname = harn.resourceName();
    LineIndex lineIndex = hfal.getLine().toLineIndex() + offsetForTesting;

    // If the file is open with the LSP manager, then use the most
    // recent copy it has sent to the server, since that is what the
    // server's line numbers will (should!) be referring to.
    if (RCSerf<LSPDocumentInfo const> docInfo =
          lspManager.getDocInfo(fname)) {
      ret.push_back(docInfo->getLastContentsCodeLine(lineIndex));
    }
    else {
      // We should have queried this file's details via VFS.
      auto const &docOrError = mapGetValueAtC(nameToDocOrError, harn);
      if (docOrError.isLeft()) {
        ret.push_back(
          docOrError.leftC()->getWholeLineStringOrRangeErrorMessage(
            lineIndex, fname));
      }
      else {
        ret.push_back(
          stringb("<Error: " << docOrError.rightC() << ">"));
      }
    }
  }
  TRACE2_GDVN_EXPRS("lspGetCodeLines", ret);

  // TODO: Create `xassertPostcondition` and use it here.
  xassert(ret.size() == locations.size());
  return ret;
}


// EOF
