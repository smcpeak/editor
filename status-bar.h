// status-bar.h
// Status bar display at the bottom of the editor window.

#ifndef EDITOR_STATUS_BAR_H
#define EDITOR_STATUS_BAR_H

#include "status-bar-fwd.h"            // fwds for this module

#include "editor-widget-fwd.h"         // EditorWidget
#include "lsp-status-widget-fwd.h"     // LSPStatusWidget

#include "smbase/refct-serf.h"         // RCSerf

#include <QWidget>

class QLabel;
class QSizeGrip;


class StatusBarDisplay : public QWidget {
public:      // data
  // The editor widget whose status is reflected here.  This is not the
  // parent widget of the status bar, it is a (child of a) sibling.
  //
  // This is non-null except while destroying the containing window.
  RCSerf<EditorWidget> m_editorWidget;

  // Controls.
  QLabel *m_cursor;                    // Cursor position.
  QLabel *m_mode;                      // Mode pixmap.  TODO: Unused!
  QLabel *m_filename;                  // Current file name.
  LSPStatusWidget *m_lspStatusWidget;  // LSP status indicator.
  QSizeGrip *m_corner;                 // Corner resize grippy.

public:      // methods
  StatusBarDisplay(EditorWidget *editorWidget, QWidget *parent = NULL);
  ~StatusBarDisplay();

  // Set the text in 'm_filename'.  This should be used instead of
  // directly modifying it so the minimum width can be adjusted.
  void setFilenameText(QString newFilename);

  // Disconnect `m_editorWidget`.
  void resetEditorWidget();
};


#endif // EDITOR_STATUS_BAR_H
