// lsp-conv.h
// Conversions between LSP data and my data.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CONV_H
#define EDITOR_LSP_CONV_H

#include "lsp-data-fwd.h"              // LSP_PublishDiagnosticsParams
#include "named-td-fwd.h"              // NamedTextDocument
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics

#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr


// Convert `lspDiags`.
stdfwd::unique_ptr<TextDocumentDiagnostics> convertLSPDiagsToTDD(
  NamedTextDocument *doc,
  LSP_PublishDiagnosticsParams const *lspDiags);


#endif // EDITOR_LSP_CONV_H
