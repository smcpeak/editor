// lsp-symbol-request-options.h
// `LSPSymbolRequestOptions`, options associated with an LSP symbol request.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_SYMBOL_REQUEST_OPTIONS_H
#define EDITOR_LSP_SYMBOL_REQUEST_OPTIONS_H

// For the moment it's just one flag.
enum class LSPSymbolRequestOptions {
  // Defaults.
  LSRO_NORMAL,

  // If the result of the request is a single location, open it in a
  // different window from the one from which the request was made.
  LSRO_OPEN_IN_OTHER_WINDOW,

  NUM_LSR_OPTIONS
};

// Return a string like "OP_NORMAL".
char const *toString(LSPSymbolRequestOptions op);

#endif // EDITOR_LSP_SYMBOL_REQUEST_OPTIONS_H
