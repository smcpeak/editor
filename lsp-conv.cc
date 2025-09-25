// lsp-conv.cc
// Code for `lsp-conv` module.

#include "lsp-conv.h"                  // this module

#include "doc-type.h"                  // DocumentType
#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams, etc.
#include "lsp-client.h"                // LSPClient
#include "lsp-version-number.h"        // LSP_VersionNumber
#include "named-td.h"                  // NamedTextDocument
#include "range-text-repl.h"           // RangeTextReplacement
#include "td-change-seq.h"             // TextDocumentChangeSequence
#include "td-change.h"                 // TextDocumentChange
#include "td-core.h"                   // TextDocumentCore
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "tdd-proposed-fix.h"          // TDD_ProposedFix
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


static TDD_Related convertLSPRelated(
  LSP_DiagnosticRelatedInformation const &lspRelated,
  URIPathSemantics semantics)
{
  return TDD_Related(
    lspRelated.m_location.m_uri.getFname(semantics),
    lspRelated.m_location.m_range.m_start.m_line,
    std::string(lspRelated.m_message) /*move*/);
}


static std::vector<TDD_Related> convertLSPRelatedList(
  std::list<LSP_DiagnosticRelatedInformation> const &relatedList,
  URIPathSemantics semantics)
{
  std::vector<TDD_Related> ret;

  for (auto const &r : relatedList) {
    ret.push_back(convertLSPRelated(r, semantics));
  }

  return ret;
}


static std::vector<TDD_ProposedFix> convertLSPProposedFixes(
  std::list<LSP_CodeAction> const &codeActions,
  URIPathSemantics pathSemantics)
{
  std::vector<TDD_ProposedFix> ret;

  for (LSP_CodeAction const &action : codeActions) {
    if (!action.m_edit) {
      // The LSP protocol has some other possibilities like renaming
      // files.  I don't currently deal with those.
      TRACE1("Proposed fix is not an edit, discarding it.");
      continue;
    }

    TDD_ProposedFix tddProposedFix(action.m_title, {});

    // Convert `action.m_edit` to `tddProposedFix.m_changesForFile`.
    for (auto const &kv : action.m_edit->m_changes) {
      LSP_FilenameURI const &fnameURI = kv.first;
      std::vector<LSP_TextEdit> const &lspTextEdits = kv.second;

      std::string fname = fnameURI.getFname(pathSemantics);
      std::vector<TDD_TextEdit> tddTextEdits;

      for (LSP_TextEdit const &lspTextEdit : lspTextEdits) {
        TextMCoordRange range = toMCoordRange(lspTextEdit.m_range);

        tddTextEdits.push_back(
          TDD_TextEdit(range, lspTextEdit.m_newText));
      }

      tddProposedFix.m_changesForFile[fname] = std::move(tddTextEdits);
    }

    ret.push_back(std::move(tddProposedFix));
  }

  return ret;
}


std::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  LSP_PublishDiagnosticsParams const *lspDiags,
  URIPathSemantics semantics)
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
      convertLSPRelatedList(lspDiag.m_relatedInformation, semantics);

    TDD_Diagnostic tddDiag(std::move(message), std::move(related));

    if (!lspDiag.m_codeActions.empty()) {
      tddDiag.m_fixes =
        convertLSPProposedFixes(lspDiag.m_codeActions, semantics);
    }

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
  LSPClient &lspClient,
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
      lspClient.uriPathSemantics(),
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
  lspClient.notify_textDocument_didChange(changeParams);
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
  LSPClient &lspClient,
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
    lspClient,
    doc,
    endPos,
    endPos,
    "//",
    true /*wantDiagnostics*/,
    "Sending no-op change part 1");

  TextMCoord newEndPos = endPos.plusBytes(ByteDifference(2));

  // Change 2: Remove the appended "//".
  lspSendOneChange(
    lspClient,
    doc,
    endPos,
    newEndPos,
    "",
    std::nullopt /*wantDiagnostics*/,
    "Sending no-op change part 2");
}


void lspSendUpdatedContents(
  LSPClient &lspClient,
  NamedTextDocument &doc)
{
  xassertPrecondition(doc.trackingChanges());
  xassertPrecondition(lspClient.isFileOpen(doc.filename()));

  RCSerf<LSPDocumentInfo const> docInfo =
    lspClient.getDocInfo(doc.filename());
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
      lspClient.uriPathSemantics(),
      version),
    convertRecordedChangesToLSPChanges(*recordedChanges));

  // Done with these.
  recordedChanges.reset();

  // Send them to the server, and have the client object update its
  // copy.
  TRACE2("Sending incremental changes: " <<
         toGDValue(changeParams).asIndentedString());
  lspClient.notify_textDocument_didChange(changeParams);

  // The document's change recorder must also know this was sent.
  doc.beginTrackingChanges();

  // If the content is unchanged, `clangd` might not send updated
  // diagnostics.  Try to persuade it to do so anyway.
  if (sameContentsAsBefore) {
    lspSendNoOpChangeWorkaround(lspClient, doc);
  }
}


std::optional<std::string> lspLanguageIdForDTOpt(DocumentType dt)
{
  switch (dt) {
    case DocumentType::DT_CPP:
      return "cpp";

    case DocumentType::DT_PYTHON:
      return "python";

    default:
      return std::nullopt;
  }
}


std::string lspLanguageIdForDT(DocumentType dt)
{
  std::optional<std::string> idOpt = lspLanguageIdForDTOpt(dt);
  xassert(idOpt.has_value());
  return *idOpt;
}


// EOF
