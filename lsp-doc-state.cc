// lsp-doc-state.cc
// Code for `lsp-doc-state` module.

#include "lsp-doc-state.h"             // this module

#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR


DEFINE_ENUMERATION_TO_STRING_OR(
  LSPDocumentState,
  NUM_LSP_DOCUMENT_STATES,
  (
    "LSPDS_NOT_OPEN",
    "LSPDS_UP_TO_DATE",
    "LSPDS_LOCAL_CHANGES",
    "LSPDS_WAITING",
    "LSPDS_RECEIVED_STALE",
  ),
  "LSPDS_invalid"
)


// EOF
