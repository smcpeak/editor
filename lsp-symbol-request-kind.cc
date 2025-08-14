// lsp-symbol-request-kind.cc
// Code for `lsp-symbol-request-kind` module.

#include "lsp-symbol-request-kind.h"   // this module

#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR, RETURN_ENUMERATION_STRING_OR


DEFINE_ENUMERATION_TO_STRING_OR(
  LSPSymbolRequestKind,
  LSPSymbolRequestKind::NUM_KINDS,
  (
    "K_DECLARATION",
    "K_DEFINITION",
    "K_HOVER_INFO",
  ),
  "<invalid kind>"
)


char const *toMessageString(LSPSymbolRequestKind lsrk)
{
  RETURN_ENUMERATION_STRING_OR(
    LSPSymbolRequestKind,
    LSPSymbolRequestKind::NUM_KINDS,
    (
      "declaration",
      "definition",
      "hover info",
    ),
    lsrk,
    "<invalid kind>"
  )
}


char const *toRequestName(LSPSymbolRequestKind lsrk)
{
  RETURN_ENUMERATION_STRING_OR(
    LSPSymbolRequestKind,
    LSPSymbolRequestKind::NUM_KINDS,
    (
      "textDocument/declaration",
      "textDocument/definition",
      "textDocument/hover",
    ),
    lsrk,
    "<invalid kind>"
  )
}


// EOF
