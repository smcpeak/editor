// lsp-conv.cc
// Code for `lsp-conv` module.

#include "lsp-conv.h"                  // this module

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams, etc.
#include "named-td.h"                  // NamedTextDocument
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "td-obs-recorder.h"           // TextDocumentChangeObservationSequence
#include "textmcoord.h"                // TextMCoord[Range]
#include "uri-util.h"                  // getFileURIPath

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/overflow.h"           // convertNumber
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition

#include <list>                        // std::list
#include <memory>                      // std::unique_ptr
#include <utility>                     // std::move

using namespace gdv;


INIT_TRACE("lsp-conv");


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


LSP_Position toLSP_Position(TextMCoord mc)
{
  // This has the same column interpretation issue as `toMCoord`.
  return LSP_Position(mc.m_line, mc.m_byteIndex);
}


// Return a range with size `n` at `pos`.
static LSP_Range rangeAtPlus(LSP_Position pos, int n)
{
  return LSP_Range(pos, pos.plusCharacters(n));
}

static LSP_Range emptyRange(LSP_Position pos)
{
  return rangeAtPlus(pos, 0);
}


static LSP_TextDocumentContentChangeEvent convertOneChange(
  TextDocumentChangeObservation const &obs)
{
  switch (obs.kind()) {
    case TextDocumentChangeObservation::OK_INSERT_LINE: {
      auto insertLine = dynamic_cast<TDCO_InsertLine const &>(obs);

      // Normally we insert at the start of the line in question.
      LSP_Position pos(insertLine.m_line, 0);

      if (insertLine.m_prevLineBytes) {
        // But if we are appending a new line, then the position at
        // that line does not exist yet.  Append to the previous line
        // instead.
        pos = LSP_Position(
          insertLine.m_line-1, *insertLine.m_prevLineBytes);
      }

      return LSP_TextDocumentContentChangeEvent(
        emptyRange(pos), std::string("\n"));
    }

    case TextDocumentChangeObservation::OK_DELETE_LINE: {
      auto deleteLine = dynamic_cast<TDCO_DeleteLine const &>(obs);

      // Normally we delete the line by extending the range forward.
      LSP_Range range(
        LSP_Position(deleteLine.m_line, 0),
        LSP_Position(deleteLine.m_line+1, 0));

      if (deleteLine.m_prevLineBytes) {
        // But if it was the last line, going forward is a no-op, so
        // go backward instead.
        range = LSP_Range(
          LSP_Position(deleteLine.m_line-1, *deleteLine.m_prevLineBytes),
          LSP_Position(deleteLine.m_line, 0));
      }

      return LSP_TextDocumentContentChangeEvent(
        range, std::string());
    }

    case TextDocumentChangeObservation::OK_INSERT_TEXT: {
      auto insertText = dynamic_cast<TDCO_InsertText const &>(obs);
      return LSP_TextDocumentContentChangeEvent(
        emptyRange(toLSP_Position(insertText.m_tc)),
        insertText.m_text);
    }

    case TextDocumentChangeObservation::OK_DELETE_TEXT: {
      auto deleteText = dynamic_cast<TDCO_DeleteText const &>(obs);
      return LSP_TextDocumentContentChangeEvent(
        rangeAtPlus(
          toLSP_Position(deleteText.m_tc),
          deleteText.m_lengthBytes),
        std::string());
    }

    case TextDocumentChangeObservation::OK_TOTAL_CHANGE: {
      auto totalChange = dynamic_cast<TDCO_TotalChange const &>(obs);
      return LSP_TextDocumentContentChangeEvent(
        std::nullopt,
        totalChange.m_contents);
    }

    // No default because I want a warning if an enumerator is missed.
  }

  xfailure("not reached");
  return LSP_TextDocumentContentChangeEvent(std::nullopt, {});
}


std::list<LSP_TextDocumentContentChangeEvent>
convertRecordedChangesToLSPChanges(
  TextDocumentChangeObservationSequence const &seq)
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


// EOF
