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


// Panel meant to be added to an EditorWindow to provide search and
// replace functionality for an EditorWidget.
class SearchAndReplacePanel : public QWidget {
private:     // data
  // Combo box where user enters "Find" string.
  QComboBox *m_findBox;

  // And the "Repl" string.
  QComboBox *m_replBox;

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

  // Put the keyboard focus on the Find box.
  void setFocusFindBox();

  // Change the Find box text, but do not scroll to first match.
  void setFindText(QString const &text);

  // QObject methods.
  virtual bool eventFilter(QObject *watched, QEvent *event) NOEXCEPT OVERRIDE;

protected:   // funcs
  // QWidget methods.
  virtual void paintEvent(QPaintEvent *event) OVERRIDE;

public slots:
  void on_findEditTextChanged(QString const &text);
};


#endif // SAR_PANEL_H
