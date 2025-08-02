// lsp-conv.cc
// Code for `lsp-conv` module.

#include "lsp-conv.h"                  // this module

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "named-td.h"                  // NamedTextDocument
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "textmcoord.h"                // TextMCoord[Range]
#include "uri-util.h"                  // getFileURIPath

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/overflow.h"           // convertNumber
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition

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
    new TextDocumentDiagnostics(diagsVersion));

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


// EOF
