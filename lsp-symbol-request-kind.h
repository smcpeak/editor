// lsp-symbol-request-kind.h
// `LSPSymbolRequestKind`, an enumeration of possible symbol requests.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_SYMBOL_REQUEST_KIND_H
#define EDITOR_LSP_SYMBOL_REQUEST_KIND_H

// Enumeration of the things we might want to know about a symbol under
// the text cursor.
enum class LSPSymbolRequestKind {
  K_DECLARATION,
  K_DEFINITION,

  NUM_KINDS
};

// Return a string like "K_DECLARATION".
char const *toString(LSPSymbolRequestKind lsrk);

// Return a string like "declaration".
char const *toMessageString(LSPSymbolRequestKind lsrk);

#endif // EDITOR_LSP_SYMBOL_REQUEST_KIND_H
