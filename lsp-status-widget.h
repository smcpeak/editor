// lsp-status-widget.h
// `LSPStatusWidget`, a status bar widget to show LSP state.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_STATUS_WIDGET_H
#define EDITOR_LSP_STATUS_WIDGET_H

#include "lsp-status-widget-fwd.h"     // fwds for this module

#include "editor-widget-fwd.h"         // EditorWidget
#include "lsp-manager-fwd.h"           // LSPManager

#include <QLabel>


// Status bar widget to show LSP state with respect to the currently
// shown file: server connection health, whether diagnostics are up to
// date, how many diagnostics there are, etc.
class LSPStatusWidget : public QLabel {
  Q_OBJECT

private:     // data
  // The file-specific status aspects (like number of diagnostics) come
  // from the file this widget is editing.  It also provides indirect
  // access to the `EditorGlobal` object.
  EditorWidget *m_editorWidget;

public:      // data
  // If not 0, then this value minus 1 is used as the protocol state
  // rather than the real state.  The point is just to see how each of
  // the states looks in the GUI.
  int m_fakeStatus;

private:     // methods
  // Set the background color.  r/g/b are in [0,255].
  void setBackgroundColor(int r, int g, int b);

public:      // methods
  ~LSPStatusWidget();

  explicit LSPStatusWidget(
    EditorWidget *editorWidget, QWidget *parent = nullptr);

  // Navigate to the global LSP manager.
  LSPManager *lspManager() const;

public Q_SLOTS:
  // Called when the `LSPManager` protocol state changes.
  void on_changedProtocolState() noexcept;
};


#endif // EDITOR_LSP_STATUS_WIDGET_H
