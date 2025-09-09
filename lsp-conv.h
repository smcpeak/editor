// lsp-conv.h
// Conversions between LSP data and editor internal data.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CONV_H
#define EDITOR_LSP_CONV_H

#include "doc-type-fwd.h"              // DocumentType [n]
#include "lsp-data-fwd.h"              // LSP_PublishDiagnosticsParams [n], LSP_TextDocumentContentChangeEvent [n]
#include "lsp-client-fwd.h"            // LSPManager [n]
#include "lsp-version-number-fwd.h"    // LSP_VersionNumber [n]
#include "named-td-fwd.h"              // NamedTextDocument [n]
#include "td-change-seq-fwd.h"         // TextDocumentChangeSequence [n]
#include "td-core-fwd.h"               // TextDocumentCore [n]
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics [n]
#include "textmcoord-fwd.h"            // TextMCoord [n]

#include "smbase/std-list-fwd.h"       // stdfwd::list [n]
#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr [n]
#include "smbase/std-optional-fwd.h"   // std::optional [n]
#include "smbase/std-string-fwd.h"     // std::string [n]

#include <cstdint>                     // std::int32_t


// Convert `lspDiags`.
//
// Requires: lspDiags->m_version.has_value()
stdfwd::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  LSP_PublishDiagnosticsParams const *lspDiags);


// Convert LSP to model coordinates.
TextMCoord toMCoord(LSP_Position const &pos);
TextMCoordRange toMCoordRange(LSP_Range const &range);


// Convert model to LSP coordinates.
LSP_Position toLSP_Position(TextMCoord mc);
LSP_Range toLSP_Range(TextMCoordRange mcr);


// Convert recorded changes to LSP changes.
stdfwd::list<LSP_TextDocumentContentChangeEvent>
convertRecordedChangesToLSPChanges(
  TextDocumentChangeSequence const &seq);


// Apply changes in `params` to `doc`.
void applyLSPDocumentChanges(
  LSP_DidChangeTextDocumentParams const &params,
  TextDocumentCore &doc);


// Incrementally send to `lspManager` the changes made to `doc` since
// the last update.
void lspSendUpdatedContents(
  LSPManager &lspManager,
  NamedTextDocument &doc);


// Return the string that LSP uses to identify `dt`, or nullopt if there
// is none, or the editor app does not know how to interact with an LSP
// server that could handle `dt`.
std::optional<std::string> lspLanguageIdForDT(DocumentType dt);


#endif // EDITOR_LSP_CONV_H
