// lsp-protocol-state.h
// `LSPProtocolState`, enumerating states of the `LSPManager` protocol.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_PROTOCOL_STATE_H
#define EDITOR_LSP_PROTOCOL_STATE_H

#include <string>                      // std::string


// Status of the `LSPManager` protocol.
//
// `LSPManager` is declared in `lsp-client.h`.
enum LSPProtocolState {
  // ---- Normal lifecycle states ----

  // Normally, we transition through these in order, cycling back to
  // "inactive" at the end.

  // `LSPManager` is inactive; both of its pointers are null.  If we
  // were active previously, the old server process has terminated.
  LSP_PS_MANAGER_INACTIVE,

  // We have sent the "initialize" request, but not received its reply.
  LSP_PS_INITIALIZING,

  // We have received the "initialize" reply, and sent the "initialized"
  // notification.  The server is operating normally and can service
  // requests.
  LSP_PS_NORMAL,

  // We have sent the "shutdown" request, but not received a reply.
  LSP_PS_SHUTDOWN1,

  // We have sent the "exit" notification, but the server process has
  // not terminated.
  LSP_PS_SHUTDOWN2,

  // ---- Error states ----

  // Any of the above states except `LSP_PS_MANAGER_INACTIVE` can
  // transition to the error state.

  // `JSON_RPC_Client` detected a protocol error.  We can't do anything
  // more with the server process.
  LSP_PS_JSON_RPC_PROTOCOL_ERROR,

  // A protocol failure occurred in the LSP layer.  It also prevents
  // further communication.
  LSP_PS_MANAGER_PROTOCOL_ERROR,

  // ---- Broken states ----

  // These states should not occur, but I defined enumerators for them
  // since `checkStatus` reports them.  There is no place in the code
  // that emits `signal_changedProtocolState` for them since a
  // transition into these states is never expected.

  // The `JSON_RPC_Client` is missing.  This is a broken state.
  LSP_PS_PROTOCOL_OBJECT_MISSING,

  // Despite the `CommandRunner` existing, it reports the server process
  // is not running.  This is a broken state.
  LSP_PS_SERVER_NOT_RUNNING,

  NUM_LSP_PROTOCOL_STATES
};

// Return a string like "LSP_PS_MANAGER_INACTIVE".
char const *toString(LSPProtocolState ps);


// Protocol state and a human-readable description of the state, which
// can have information beyond what is in `m_protocolState`.
class LSPAnnotatedProtocolState {
public:      // data
  // Basic state.
  LSPProtocolState m_protocolState;

  // Annotation/description.
  std::string m_description;

public:
  ~LSPAnnotatedProtocolState();
  LSPAnnotatedProtocolState(LSPProtocolState ps, std::string &&desc);
};


#endif // EDITOR_LSP_PROTOCOL_STATE_H
