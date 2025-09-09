// lsp-conv.cc
// Code for `lsp-conv` module.

#include "lsp-conv.h"                  // this module

#include "doc-type.h"                  // DocumentType
#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams, etc.
#include "lsp-manager.h"               // LSPManager
#include "lsp-version-number.h"        // LSP_VersionNumber
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
#include "smbase/sm-env.h"             // envAsBool
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition

#include <list>                        // std::list
#include <memory>                      // std::unique_ptr
#include <optional>                    // std::{nullopt, optional}
#include <string>                      // std::string
#include <utility>                     // std::move

using namespace gdv;
using namespace smbase;


INIT_TRACE("lsp-conv");


TDD_Related convertLSPRelated(
  LSP_DiagnosticRelatedInformation const &lspRelated)
{
  return TDD_Related(
    lspRelated.m_location.m_uri.getFname(),
    lspRelated.m_location.m_range.m_start.m_line,
    std::string(lspRelated.m_message) /*move*/);
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

  TD_VersionNumber diagsVersion =
    lspDiags->m_version->toTD_VersionNumber();

  std::unique_ptr<TextDocumentDiagnostics> ret(
    new TextDocumentDiagnostics(diagsVersion, std::nullopt));

  for (LSP_Diagnostic const &lspDiag : lspDiags->m_diagnostics) {
    TextMCoordRange range = toMCoordRange(lspDiag.m_range);

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
  return TextMCoord(pos.m_line, pos.m_character);
}


TextMCoordRange toMCoordRange(LSP_Range const &range)
{
  return TextMCoordRange(
    toMCoord(range.m_start), toMCoord(range.m_end));
}


LSP_Position toLSP_Position(TextMCoord mc)
{
  return LSP_Position(LineIndex(mc.m_line), mc.m_byteIndex);
}


LSP_Range toLSP_Range(TextMCoordRange mcr)
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


// As part of a `clangd` workaround, send a single change notification.
static void lspSendOneChange(
  LSPManager &lspManager,
  NamedTextDocument &doc,
  TextMCoord start,
  TextMCoord end,
  char const *newText,
  std::optional<bool> wantDiagnostics,
  char const *traceLabel)
{
  doc.bumpVersionNumber();

  LSP_DidChangeTextDocumentParams changeParams(
    LSP_VersionedTextDocumentIdentifier::fromFname(
      doc.filename(),
      LSP_VersionNumber::fromTDVN(doc.getVersionNumber())),
    std::list<LSP_TextDocumentContentChangeEvent>{
      LSP_TextDocumentContentChangeEvent(
        toLSP_Range(TextMCoordRange(start, end)),
        newText)
    },
    wantDiagnostics
  );

  TRACE1(traceLabel << ": " <<
         toGDValue(changeParams).asIndentedString());
  lspManager.notify_textDocument_didChange(changeParams);
  doc.beginTrackingChanges();
}


// If the content is unchanged, and the preamble is also unchanged
// (which we can't easily tell), `clangd` will not send updated
// diagnostics.  As a workaround to force updated diagnostics, send a
// series of two quick changes that are together a no-op.  For the first
// change, disable `clangd`s usual change aggregation so it will emit
// diagnostics for that version.  Then the second will also get
// diagnostics soon afterward.
static void lspSendNoOpChangeWorkaround(
  LSPManager &lspManager,
  NamedTextDocument &doc)
{
  // Provide a way to disable my workaround so I can keep experimenting
  // with fixing `clangd` itself.
  static bool disableWorkaround =
    envAsBool("LSP_CONV_DISABLE_NO_OP_CHANGE_WORKAROUND");
  if (disableWorkaround) {
    return;
  }

  TextMCoord endPos = doc.endCoord();

  // Change 1: Append "//", which should have minimal adverse impact, at
  // least for C/C++.
  lspSendOneChange(
    lspManager,
    doc,
    endPos,
    endPos,
    "//",
    true /*wantDiagnostics*/,
    "Sending no-op change part 1");

  TextMCoord newEndPos = endPos.plusBytes(ByteDifference(2));

  // Change 2: Remove the appended "//".
  lspSendOneChange(
    lspManager,
    doc,
    endPos,
    newEndPos,
    "",
    std::nullopt /*wantDiagnostics*/,
    "Sending no-op change part 2");
}


void lspSendUpdatedContents(
  LSPManager &lspManager,
  NamedTextDocument &doc)
{
  xassertPrecondition(doc.trackingChanges());
  xassertPrecondition(lspManager.isFileOpen(doc.filename()));

  RCSerf<LSPDocumentInfo const> docInfo =
    lspManager.getDocInfo(doc.filename());
  xassert(docInfo);

  // This can throw `XNumericConversion`.
  LSP_VersionNumber version =
    LSP_VersionNumber::fromTDVN(doc.getVersionNumber());

  if (docInfo->m_lastSentVersion == version) {
    TRACE1("LSP: While updating " << doc.documentName() <<
           ": previous version is " << docInfo->m_lastSentVersion <<
           ", same as new version; bumping to force re-analysis.");

    // We want to re-send despite no content changes, for example
    // because a header file changed that should fix issues in the
    // current file.  Bump the version and try again.
    doc.bumpVersionNumber();
    version = LSP_VersionNumber::fromTDVN(doc.getVersionNumber());
  }

  // Are the contents the same?  If so we use a workaround below.
  bool const sameContentsAsBefore =
    docInfo->lastContentsEquals(doc.getCore());

  if (!( version > docInfo->m_lastSentVersion )) {
    // Sending this would be a protocol violation.
    xmessage(stringb(
      "The current document version (" << version <<
      ") is not greater than the previously sent document version (" <<
      docInfo->m_lastSentVersion << ")."));
  }

  // Get the recorded changes.
  RCSerf<TextDocumentChangeSequence const> recordedChanges =
    doc.getUnsentChanges();

  // Convert changes to the LSP format and package them into a
  // "didChange" params structure.
  LSP_DidChangeTextDocumentParams changeParams(
    LSP_VersionedTextDocumentIdentifier::fromFname(
      doc.filename(),
      version),
    convertRecordedChangesToLSPChanges(*recordedChanges));

  // Done with these.
  recordedChanges.reset();

  // Send them to the server, and have the manager update its copy.
  TRACE2("Sending incremental changes: " <<
         toGDValue(changeParams).asIndentedString());
  lspManager.notify_textDocument_didChange(changeParams);

  // The document's change recorder must also know this was sent.
  doc.beginTrackingChanges();

  // If the content is unchanged, `clangd` might not send updated
  // diagnostics.  Try to persuade it to do so anyway.
  if (sameContentsAsBefore) {
    lspSendNoOpChangeWorkaround(lspManager, doc);
  }
}


std::optional<std::string> lspLanguageIdForDT(DocumentType dt)
{
  switch (dt) {
    case DocumentType::DT_C:
      // `DT_C` means C and C++, and nearly always that means C++.
      return "cpp";

    case DocumentType::DT_PYTHON:
      return "python";

    default:
      return std::nullopt;
  }
}


// EOF
