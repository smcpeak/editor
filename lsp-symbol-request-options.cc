// lsp-symbol-request-options.cc
// Code for `lsp-symbol-request-options` module.

#include "lsp-symbol-request-options.h"          // this module

#include "smbase/sm-macros.h"                    // DEFINE_ENUMERATION_TO_STRING_OR, RETURN_ENUMERATION_STRING_OR


DEFINE_ENUMERATION_TO_STRING_OR(
  LSPSymbolRequestOptions,
  LSPSymbolRequestOptions::NUM_LSR_OPTIONS,
  (
    "LSRO_NORMAL",
    "LSRO_OPEN_IN_OTHER_WINDOW",
  ),
  "<invalid options>"
)


// EOF
