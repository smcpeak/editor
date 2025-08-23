// lsp-conv.cc
// Code for `lsp-conv` module.

#include "lsp-conv.h"                  // this module

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams, etc.
#include "lsp-manager.h"               // LSPManager
#include "named-td.h"                  // NamedTextDocument
#include "range-text-repl.h"           // RangeTextReplacement
#include "td-change-seq.h"             // TextDocumentChangeSequence
#include "td-change.h"                 // TextDocumentChange
#include "td-core.h"                   // TextDocumentCore
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "textmcoord.h"                // TextMCoord[Range]
#include "uri-util.h"                  // getFileURIPath

#include "smbase/ast-switch.h"         // ASTSWITCHC
#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/optional-util.h"      // smbase::optInvoke
#include "smbase/overflow.h"           // convertNumber
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition

#include <list>                        // std::list
#include <memory>                      // std::unique_ptr
#include <utility>                     // std::move

using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-conv");


LSP_VersionNumber toLSP_VersionNumber(std::uint64_t n)
{
  return convertNumber<LSP_VersionNumber>(n);
}


TextMCoord convertLSPPosition(LSP_Position const &pos)
{
  // TODO: This is wrong in the presence of UTF-8.  The solution I have
  // in mind is to negotiate to use bytes with the server.
  return TextMCoord(pos.m_line, pos.m_character);
}


TextMCoordRange convertLSPRange(LSP_Range const &range)
{
  return TextMCoordRange(
    convertLSPPosition(range.m_start),
    convertLSPPosition(range.m_end));
}


TDD_Related convertLSPRelated(
  LSP_DiagnosticRelatedInformation const &lspRelated)
{
  return TDD_Related(
    getFileURIPath(lspRelated.m_location.m_uri),
    lspRelated.m_location.m_range.m_start.m_line+1,
    std::string(lspRelated.m_message));
}


std::vector<TDD_Related> convertLSPRelatedList(
  std::list<LSP_DiagnosticRelatedInformation> const &relatedList)
{
  std::vector<TDD_Related> ret;

  for (auto const &r : relatedList) {
    ret.push_back(convertLSPRelated(r));
  }

  return ret;
}



std::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  LSP_PublishDiagnosticsParams const *lspDiags)
{
  xassertPrecondition(lspDiags->m_version.has_value());

  TextDocument::VersionNumber diagsVersion =
    convertNumber<TextDocument::VersionNumber>(*lspDiags->m_version);

  std::unique_ptr<TextDocumentDiagnostics> ret(
    new TextDocumentDiagnostics(diagsVersion, std::nullopt));

  for (LSP_Diagnostic const &lspDiag : lspDiags->m_diagnostics) {
    TextMCoordRange range = convertLSPRange(lspDiag.m_range);

    std::string message = lspDiag.m_message;
    std::vector<TDD_Related> related =
      convertLSPRelatedList(lspDiag.m_relatedInformation);

    TDD_Diagnostic tddDiag(std::move(message), std::move(related));

    ret->insertDiagnostic(range, std::move(tddDiag));
  }

  return ret;
}


TextMCoord toMCoord(LSP_Position const &pos)
{
  // TODO: This assumes that the units are the same, but I have not
  // properly negotiated that.
  return TextMCoord(pos.m_line, pos.m_character);
}


TextMCoordRange toMCoordRange(LSP_Range const &range)
{
  return TextMCoordRange(
    toMCoord(range.m_start), toMCoord(range.m_end));
}


static LSP_Position toLSP_Position(TextMCoord mc)
{
  // This has the same column interpretation issue as `toMCoord`.
  return LSP_Position(mc.m_line, mc.m_byteIndex);
}


static LSP_Range toLSP_Range(TextMCoordRange mcr)
{
  return LSP_Range(
    toLSP_Position(mcr.m_start),
    toLSP_Position(mcr.m_end));
}


static LSP_TextDocumentContentChangeEvent convertOneChange(
  TextDocumentChange const &obs)
{
  RangeTextReplacement rtr = obs.getRangeTextReplacement();

  return LSP_TextDocumentContentChangeEvent(
    optInvoke(toLSP_Range, rtr.m_range),
    rtr.m_text);
}


std::list<LSP_TextDocumentContentChangeEvent>
convertRecordedChangesToLSPChanges(
  TextDocumentChangeSequence const &seq)
{
  std::list<LSP_TextDocumentContentChangeEvent> ret;

  for (auto const &ptr : seq.m_seq) {
    ret.push_back(convertOneChange(*ptr));
  }

  return ret;
}


void applyOneLSPDocumentChange(
  LSP_TextDocumentContentChangeEvent const &change,
  TextDocumentCore &doc)
{
  if (!change.m_range.has_value()) {
    doc.replaceWholeFileString(change.m_text);
    return;
  }

  TextMCoordRange range = toMCoordRange(*change.m_range);
  TextMCoordRange adjustedRange = range;
  if (doc.adjustMCoordRange(adjustedRange /*INOUT*/)) {
    TRACE1_GDVN_EXPRS("applyOneLSPDocumentChange adjusted range",
      range, adjustedRange);
  }
  doc.replaceMultilineRange(adjustedRange, change.m_text);
}


void applyLSPDocumentChanges(
  LSP_DidChangeTextDocumentParams const &params,
  TextDocumentCore &doc)
{
  for (LSP_TextDocumentContentChangeEvent const &change :
         params.m_contentChanges) {
    applyOneLSPDocumentChange(change, doc);
  }
}


void lspSendUpdatedContents(
  LSPManager &lspManager,
  NamedTextDocument &doc)
{
  xassertPrecondition(doc.trackingChanges());
  xassertPrecondition(lspManager.isFileOpen(doc.filename()));

  // Get the recorded changes.
  RCSerf<TextDocumentChangeSequence const> recordedChanges =
    doc.getUnsentChanges();

  // Convert changes to the LSP format and package them into a
  // "didChange" params structure.
  LSP_DidChangeTextDocumentParams changeParams(
    LSP_VersionedTextDocumentIdentifier::fromFname(
      doc.filename(),
      toLSP_VersionNumber(doc.getVersionNumber())),
    convertRecordedChangesToLSPChanges(*recordedChanges));

  // Done with these.
  recordedChanges.reset();

  // Send them to the server, and have the manager update its copy.
  TRACE2("Sending incremental changes: " <<
         toGDValue(changeParams).asIndentedString());
  lspManager.notify_textDocument_didChange(changeParams);

  // The document's change recorder must also know this was sent.
  doc.beginTrackingChanges();
}


// EOF
