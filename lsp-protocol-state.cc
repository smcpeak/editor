// lsp-protocol-state.cc
// Code for `lsp-protocol-state` module.

#include "lsp-protocol-state.h"        // this module

#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR

#include <iostream>                    // std::ostream
#include <string>                      // std::string
#include <utility>                     // std::move


// ------------------------- LSPProtocolState --------------------------
DEFINE_ENUMERATION_TO_STRING_OR(
  LSPProtocolState,
  NUM_LSP_PROTOCOL_STATES,
  (
    "LSP_PS_CLIENT_INACTIVE",
    "LSP_PS_INITIALIZING",
    "LSP_PS_NORMAL",
    "LSP_PS_SHUTDOWN1",
    "LSP_PS_SHUTDOWN2",

    "LSP_PS_JSON_RPC_PROTOCOL_ERROR",
    "LSP_PS_LSP_PROTOCOL_ERROR",

    "LSP_PS_PROTOCOL_OBJECT_MISSING",
    "LSP_PS_SERVER_NOT_RUNNING",
  ),
  "Unknown LSP"
)


std::ostream &operator<<(std::ostream &os, LSPProtocolState ps)
{
  return os << toString(ps);
}


// --------------------- LSPAnnotatedProtocolState ---------------------
LSPAnnotatedProtocolState::~LSPAnnotatedProtocolState()
{}


LSPAnnotatedProtocolState::LSPAnnotatedProtocolState(
  LSPProtocolState ps,
  std::string &&desc)
  : m_protocolState(ps),
    m_description(std::move(desc))
{}


// EOF
