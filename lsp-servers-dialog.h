// lsp-servers-dialog.h
// `LSPServersDialog`, showing a list of LSP servers.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_SERVERS_DIALOG_H
#define EDITOR_LSP_SERVERS_DIALOG_H

#include "lsp-servers-dialog-fwd.h"              // fwds for this module

#include "lsp-client-manager-fwd.h"              // LSPClientManager

#include "smqtutil/sm-table-widget-fwd.h"        // SMTableWidget

#include "smbase/refct-serf.h"                   // SerfRefCount, NNRCSerf, RCSerfOpt

#include <QDialog>


// Dialog to show a list of current LSP server scopes and allow certain
// manipulations.
//
// See doc/lsp-servers-dialog.ded.png for a rough wireframe.
class LSPServersDialog
  : public QDialog,
    public SerfRefCount {
  Q_OBJECT

private:     // data
  // The manager containing the data to show and operations to perform
  // upon it.
  NNRCSerf<LSPClientManager> m_lspClientManager;

  // Table control with list of servers.
  SMTableWidget *m_table = nullptr;

private:     // data
  // Get the currently selected server.  If none is, pop up an info box
  // and return null.
  RCSerfOpt<ScopedLSPClient const> getSelectedServer();

private Q_SLOTS:
  // React to button presses.
  void showHelp() noexcept;
  void startSelectedServer() noexcept;
  void stopSelectedServer() noexcept;
  void openSelectedStderr() noexcept;

public:
  ~LSPServersDialog();

  explicit LSPServersDialog(NNRCSerf<LSPClientManager> lspClientManager);

public Q_SLOTS:
  // Populate table with data from `m_lspClientManager`.
  void repopulateTable() noexcept;

Q_SIGNALS:
  // The dialog user wants to open `fname` in the editor.
  void signal_openFileInEditor(std::string fname);
};


#endif // EDITOR_LSP_SERVERS_DIALOG_H
