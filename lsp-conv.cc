// lsp-conv.cc
// Code for `lsp-conv` module.

#include "lsp-conv.h"                  // this module

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "named-td.h"                  // NamedTextDocument
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "textmcoord.h"                // TextMCoord[Range]

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition

#include <memory>                      // std::unique_ptr

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


std::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  NamedTextDocument *doc,
  LSP_PublishDiagnosticsParams const *lspDiags)
{
  xassertPrecondition(lspDiags->m_version.has_value());

  TextDocument::VersionNumber diagsVersion =
    static_cast<TextDocument::VersionNumber>(*lspDiags->m_version);

  std::unique_ptr<TextDocumentDiagnostics> ret(
    new TextDocumentDiagnostics(diagsVersion, doc));

  for (LSP_Diagnostic const &diag : lspDiags->m_diagnostics) {
    TextMCoordRange range = convertLSPRange(diag.m_range);
    if (ret->insertWithAdjust(range, std::string(diag.m_message))) {
      TRACE1("adjusted LSP range " << toGDValue(range) <<
             " to " << toGDValue(range));
    }
  }

  return ret;
}


// EOF
