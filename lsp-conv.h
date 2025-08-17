// lsp-conv.h
// Conversions between LSP data and my data.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CONV_H
#define EDITOR_LSP_CONV_H

#include "lsp-data-fwd.h"              // LSP_PublishDiagnosticsParams
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics
#include "textmcoord.h"                // TextMCoord

#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr


// Convert `lspDiags`.
//
// Requires: lspDiags->m_version.has_value()
stdfwd::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  LSP_PublishDiagnosticsParams const *lspDiags);


// Convert LSP to model coordinates.
TextMCoord toMCoord(LSP_Position const &pos);
TextMCoordRange toMCoordRange(LSP_Range const &range);


#endif // EDITOR_LSP_CONV_H
