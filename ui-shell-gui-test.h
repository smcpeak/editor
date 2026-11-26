// ui-shell-gui-test.h
// Classes for `ui-shell-gui-test`, a program for testing the GUI UI
// shell commands.  See doc/plans/claude-ui-shell-gui-test.txt.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_GUI_TEST_H
#define EDITOR_UI_SHELL_GUI_TEST_H

// editor
#include "ui-shell-gui-queryable.h"              // UIShellGUIQueryable

// smbase
#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue [f]
#include "smbase/sm-macros.h"                    // NO_OBJECT_COPIES
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/sm-override.h"                  // OVERRIDE
#include "smbase/std-string-fwd.h"               // std::string [f]

// Qt
#include <QDialog>
#include <QFrame>
#include <QMainWindow>

class QAction;
class QComboBox;
class QImage;
class QKeyEvent;
class QLabel;
class QLineEdit;
class QListView;
class QListWidget;
class QMenu;
class QMouseEvent;
class QPushButton;
class QString;
class QTableWidget;
class QTextEdit;
class QTimer;
class QWidget;


// A focusable widget that reports the key events it receives by
// setting the text of a label.
class UISGTKeyLogWidget : public QFrame {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTKeyLogWidget);

public:      // data
  // Where the events are reported.  Not owned.
  QLabel *m_label;

private:     // funcs
  void reportKeyEvent(char const *kind, QKeyEvent *event);

protected:   // funcs
  virtual void keyPressEvent(QKeyEvent *event) OVERRIDE;
  virtual void keyReleaseEvent(QKeyEvent *event) OVERRIDE;

public:      // funcs
  UISGTKeyLogWidget(QLabel *label, QWidget *parent);
  ~UISGTKeyLogWidget();

  // Text of `m_label` before any event.
  static QString initialLabelText();
};


// A fixed-size widget that reports the mouse events it receives by
// setting the text of a label.
class UISGTMousePad : public QFrame {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTMousePad);

public:      // data
  // Where the events are reported.  Not owned.
  QLabel *m_label;

private:     // funcs
  void reportMouseEvent(char const *kind, QMouseEvent *event);

protected:   // funcs
  virtual void mousePressEvent(QMouseEvent *event) OVERRIDE;
  virtual void mouseReleaseEvent(QMouseEvent *event) OVERRIDE;
  virtual void mouseDoubleClickEvent(QMouseEvent *event) OVERRIDE;
  virtual void mouseMoveEvent(QMouseEvent *event) OVERRIDE;

public:      // funcs
  UISGTMousePad(QLabel *label, QWidget *parent);
  ~UISGTMousePad();

  // Text of `m_label` before any event.
  static QString initialLabelText();
};


// Modal dialog for editing a name.
class UISGTNameDialog : public QDialog {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTNameDialog);

public:      // data
  // Widgets.  They are owned by their Qt parents.
  QLineEdit *m_nameEdit;
  QPushButton *m_okButton;
  QPushButton *m_cancelButton;

public:      // funcs
  // Initially, the name is `name`.
  UISGTNameDialog(QString const &name, QWidget *parent);
  ~UISGTNameDialog();

  // The current name.
  QString name() const;
};


// Modal dialog with a table that rows can be added to.
class UISGTTableDialog : public QDialog {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTTableDialog);

public:      // data
  // Widgets.  They are owned by their Qt parents.
  QTableWidget *m_table;
  QPushButton *m_addRowButton;
  QPushButton *m_closeButton;

private:     // funcs
  void appendRow(QString const &name, QString const &value);

public:      // funcs
  explicit UISGTTableDialog(QWidget *parent);
  ~UISGTTableDialog();

public Q_SLOTS:
  void on_addRowButtonClicked() NOEXCEPT;
};


// Modeless window with lists.
class UISGTListWindow : public QWidget {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTListWindow);

public:      // data
  // Number of the next item that `m_addItemButton` adds.
  int m_nextItemNumber;

  // Widgets.  They are owned by their Qt parents.
  QListWidget *m_list;
  QPushButton *m_addItemButton;
  QPushButton *m_removeItemButton;
  QListView *m_listView;

public:      // funcs
  // `parent` is the window that owns this one; this is nonetheless a
  // separate top-level window.
  explicit UISGTListWindow(QWidget *parent);
  ~UISGTListWindow();

public Q_SLOTS:
  void on_addItemButtonClicked() NOEXCEPT;
  void on_removeItemButtonClicked() NOEXCEPT;
};


// The main window of the test program.
class UISGTMainWindow : public QMainWindow,
                        public UIShellGUIQueryable {
  Q_OBJECT
  NO_OBJECT_COPIES(UISGTMainWindow);

public:      // types
  // State of the delayed update started by `m_delayButton`.
  enum DelayState {
    DS_IDLE,
    DS_PENDING,
    DS_DONE,
  };

public:      // data
  // Number of times `m_clickButton` has been clicked.
  int m_clickCount;

  // State of the delayed update.
  DelayState m_delayState;

  // Timer for the delayed update.
  QTimer *m_delayTimer;

  // Widgets and actions.  They are owned by their Qt parents.
  QWidget *m_central;
  QPushButton *m_clickButton;
  QLabel *m_clickCountLabel;
  QLabel *m_nameLabel;
  QLineEdit *m_nameEdit;
  QPushButton *m_copyButton;
  QLabel *m_nameResultLabel;
  QLabel *m_featureLabel;
  QPushButton *m_delayButton;
  QLabel *m_delayLabel;
  QLabel *m_messageLabel;
  QComboBox *m_colorCombo;
  QLabel *m_keyLogLabel;
  UISGTKeyLogWidget *m_keyLog;
  QLabel *m_mouseLabel;
  UISGTMousePad *m_mousePad;
  QTextEdit *m_notes;

  // The List Window.  It is created hidden.
  UISGTListWindow *m_listWindow;

  QAction *m_quitAction;
  QAction *m_featureAction;
  QAction *m_resetAction;
  QAction *m_editNameAction;
  QAction *m_tableAction;
  QAction *m_messageBoxAction;
  QAction *m_listWindowAction;

private:     // funcs
  void buildMenus();

  // Add a menu with `title` and `name` to the menu bar.
  QMenu *addMenu(char const *title, char const *name);

  // Add an action to `menu`.
  QAction *addAction(QMenu *menu, char const *title, char const *name,
                     char const *shortcut);

  // Texts that the labels should have.
  QString clickCountLabelText() const;
  QString featureLabelText() const;
  QString delayLabelText() const;

  static char const *delayStateString(DelayState state);

public:      // funcs
  UISGTMainWindow();
  ~UISGTMainWindow();

  // Check that the labels agree with the state they display.  This is
  // the global self-check for the UI shell.
  void selfCheck() const;

  // UIShellQueryable methods.
  virtual gdv::GDValue uiShellQuery(std::string const &state) OVERRIDE;

  // UIShellGUIQueryable methods.
  virtual QImage uiShellImage(std::string const &what) OVERRIDE;

public Q_SLOTS:
  void on_clickButtonClicked() NOEXCEPT;
  void on_copyButtonClicked() NOEXCEPT;
  void on_delayButtonClicked() NOEXCEPT;
  void on_delayTimerTimeout() NOEXCEPT;
  void on_quitActionTriggered() NOEXCEPT;
  void on_featureActionToggled(bool checked) NOEXCEPT;
  void on_resetActionTriggered() NOEXCEPT;
  void on_editNameActionTriggered() NOEXCEPT;
  void on_tableActionTriggered() NOEXCEPT;
  void on_messageBoxActionTriggered() NOEXCEPT;
  void on_listWindowActionTriggered() NOEXCEPT;
};


#endif // EDITOR_UI_SHELL_GUI_TEST_H
