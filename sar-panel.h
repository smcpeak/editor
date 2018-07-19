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
class QCheckBox;
class QLabel;
class QToolButton;


// Panel meant to be added to an EditorWindow to provide search and
// replace functionality for an EditorWidget.
class SearchAndReplacePanel : public QWidget {
  Q_OBJECT

private:     // data
  // Label with information about current matches.
  QLabel *m_matchStatusLabel;

  // Combo box where user enters "Find" string.
  QComboBox *m_findBox;

  // And the "Repl" string.
  QComboBox *m_replBox;

  // Checkbox "E", meaning regular Expression.
  QCheckBox *m_regexCheckBox;

  // "Help" button.
  QToolButton *m_helpButton;

  // The editor we are interacting with.
  //
  // This is NULL during while both objects are being constructed or
  // destroyed, but while we are receiving UI events, it is assumed to
  // be not NULL.
  //
  // This is a sibling widget, and therefore the common wisdom would be
  // to interact with it using signals instead of direct calls.  But the
  // coupling between these two is necessarily tight due to the UI
  // design, so in this instance I think it works better to store a
  // pointer and make direct method calls.
  RCSerf<EditorWidget> m_editorWidget;

  // When true, ignore the 'findEditTextChanged' signal.
  bool m_ignore_findEditTextChanged;

  // When true, we are responding to a broadcast change, and therefore
  // should not initiate one of our own.
  bool m_handlingBroadcastChange;

private:     // funcs
  // Add the current Find and Repl strings to their respective
  // histories, removing duplicates and trimming overlong histories.
  void rememberFindReplStrings();

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

  // The search panel in one window changed.  This is called to update
  // the others.
  void searchPanelChanged(SearchAndReplacePanel *panel);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) NOEXCEPT OVERRIDE;

protected:   // funcs
  // QWidget methods.
  virtual void paintEvent(QPaintEvent *event) OVERRIDE;

private Q_SLOTS:
  void slot_findEditTextChanged(QString const &text) NOEXCEPT;
  void slot_replEditTextChanged(QString const &text) NOEXCEPT;
  void slot_regexStateChanged(int state) NOEXCEPT;
  void slot_help() NOEXCEPT;

Q_SIGNALS:
  // Emitted when a control in the panel changes.
  void signal_searchPanelChanged(SearchAndReplacePanel *panel);
};


#endif // SAR_PANEL_H
