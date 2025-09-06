// lsp-status-widget.h
// `LSPStatusWidget`, a status bar widget to show LSP state.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_STATUS_WIDGET_H
#define EDITOR_LSP_STATUS_WIDGET_H

#include "lsp-status-widget-fwd.h"     // fwds for this module

#include "editor-global-fwd.h"         // EditorGlobal
#include "editor-widget-fwd.h"         // EditorWidget
#include "lsp-manager-fwd.h"           // LSPManager

#include "smbase/refct-serf.h"         // RCSerf

#include <QColor>
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
  //
  // This is non-null except while destroying the containing window.
  RCSerf<EditorWidget> m_editorWidget;

  // Background color to draw.
  QColor m_bgColor;

  // Message for the status report dialog.  This is built while doing
  // normal status updates, but only shown if the user clicks on the
  // widget.
  std::string m_statusReport;

public:      // data
  // If not 0, then this value minus 1 is used as the protocol state
  // rather than the real state.  The point is just to see how each of
  // the states looks in the GUI.
  int m_fakeStatus;

protected:   // methods
  // QWidget overrides.
  virtual void mousePressEvent(QMouseEvent *e) override;
  virtual void paintEvent(QPaintEvent *e) override;

public:      // methods
  ~LSPStatusWidget();

  explicit LSPStatusWidget(
    EditorWidget *editorWidget, QWidget *parent = nullptr);

  // Get the global editor object.
  EditorGlobal *editorGlobal() const;

  // Nullify `m_editorWidget` and disconnect any signals.
  void resetEditorWidget();

public Q_SLOTS:
  // Called when something changes that potentially affects the LSP
  // status display.
  void on_changedLSPStatus() noexcept;
};


#endif // EDITOR_LSP_STATUS_WIDGET_H
