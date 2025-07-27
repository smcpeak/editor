// lsp-doc-state.h
// `LSPDocumentState`, the state of one document w.r.t. LSP.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_DOC_STATE_H
#define EDITOR_LSP_DOC_STATE_H


// The state of one document w.r.t. LSP.
enum LSPDocumentState {
  // Document is not open w.r.t. the LSP server.
  LSPDS_NOT_OPEN,

  // We have LSP diagnostics and they correspond to the current version
  // of the file being edited.
  LSPDS_UP_TO_DATE,

  // The user has made changes to the document since the version from
  // which the diagnostics were derived, so they are potentially stale.
  LSPDS_LOCAL_CHANGES,

  // We have notified the server of the current file version, but it has
  // not yet provided the resulting diagnostics.
  LSPDS_WAITING,

  // We received stale diagnostics and discarded them, meaning we need
  // to try again.  (TODO: This should be fixed and removed.)
  LSPDS_RECEIVED_STALE,

  NUM_LSP_DOCUMENT_STATES
};


#endif // EDITOR_LSP_DOC_STATE_H
