// lsp-conv.h
// Conversions between LSP data and editor internal data.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CONV_H
#define EDITOR_LSP_CONV_H

#include "lsp-data-fwd.h"              // LSP_PublishDiagnosticsParams [n], LSP_TextDocumentContentChangeEvent [n]
#include "td-change-seq-fwd.h"         // TextDocumentChangeSequence [n]
#include "td-core-fwd.h"               // TextDocumentCore [n]
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics [n]
#include "textmcoord-fwd.h"            // TextMCoord [n]

#include "smbase/std-list-fwd.h"       // stdfwd::list [n]
#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr [n]


// Convert `lspDiags`.
//
// Requires: lspDiags->m_version.has_value()
stdfwd::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  LSP_PublishDiagnosticsParams const *lspDiags);


// Convert LSP to model coordinates.
TextMCoord toMCoord(LSP_Position const &pos);
TextMCoordRange toMCoordRange(LSP_Range const &range);


// Convert recorded changes to LSP changes.
stdfwd::list<LSP_TextDocumentContentChangeEvent>
convertRecordedChangesToLSPChanges(
  TextDocumentChangeSequence const &seq);


// Apply changes in `params` to `doc`.
void applyLSPDocumentChanges(
  LSP_DidChangeTextDocumentParams const &params,
  TextDocumentCore &doc);


#endif // EDITOR_LSP_CONV_H
