// sar-panel.h
// SearchAndReplacePanel

#ifndef SAR_PANEL_H
#define SAR_PANEL_H

// smbase
#include "refct-serf.h"                // RCSerf
#include "sm-override.h"               // OVERRIDE

// Qt
#include <QWidget>

class EditorWidget;                    // editor-widget.h

class QComboBox;
class QLabel;
class QToolButton;


// Panel meant to be added to an EditorWindow to provide search and
// replace functionality for an EditorWidget.
class SearchAndReplacePanel : public QWidget {
  Q_OBJECT

private:     // data
  // Combo box where user enters "Find" string.
  QComboBox *m_findBox;

  // And the "Repl" string.
  QComboBox *m_replBox;

  // Label with information about current matches.
  QLabel *m_matchStatusLabel;

  // "Help" button.
  QToolButton *m_helpButton;

  // The editor we are interacting with.
  RCSerf<EditorWidget> m_editorWidget;

  // When true, ignore the 'findEditTextChanged' signal.
  bool m_ignore_findEditTextChanged;

public:      // funcs
  SearchAndReplacePanel(QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~SearchAndReplacePanel();

  // Associate with our EW, which cannot be done during construction
  // because of an issue with the order in which certain objects are
  // made.
  void setEditorWidget(EditorWidget *w);

  // If the editor has focus, switch to SAR and prepare to search.
  // Otherwise give focus back to the editor.
  void toggleSARFocus();

  // Put the keyboard focus on the Find box.
  void setFocusFindBox();

  // True if the Find box has at least one character in it that is not
  // currently selected.
  bool findHasNonSelectedText() const;

  // Change the Find box text, but do not scroll to first match.
  void setFindText(QString const &text);

  // Set the editor's hit text to what is in m_findBox, and optionally
  // scroll the first hit into view.
  void updateEditorHitText(bool scrollToHit);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) NOEXCEPT OVERRIDE;

protected:   // funcs
  // QWidget methods.
  virtual void paintEvent(QPaintEvent *event) OVERRIDE;

public slots:
  void on_findEditTextChanged(QString const &text);
  void on_help();
};


#endif // SAR_PANEL_H
