// textinput.h
// TextInputDialog class.

#ifndef EDITOR_TEXTINPUT_H
#define EDITOR_TEXTINPUT_H

// editor
#include "modal-dialog.h"              // ModalDialog

// smbase
#include "smbase/sm-override.h"        // OVERRIDE

// Qt
#include <QString>
#include <QStringList>

class QComboBox;
class QLabel;
class QVBoxLayout;


// Prompt for a single-line text input.  Remembers history of previous
// inputs.
class TextInputDialog : public ModalDialog {
  Q_OBJECT

protected:   // data
  // The main vbox containing the controls.  Storing this allows a
  // derived class to add additional controls.
  //
  // I might want to move this into 'ModalDialog'.
  QVBoxLayout *m_vbox;

  // Position in 'm_vbox' where additional controls should be added
  // by derived classes.
  int m_vboxNextIndex;

  // Label above the input control.
  QLabel *m_label;

  // The combo box control with editable text and history.
  QComboBox *m_comboBox;

public:      // data
  // History of prior choices, from most to least recent.  This is *not*
  // automatically populated; the user must set it, either directly or
  // with 'rememberInput'.  The first entry is also the default value
  // for the input text box.
  QStringList m_history;

  // Used by 'rememberInput', this is maximum number of history
  // elements, after which point the oldest entry is removed.  This is
  // initially 20 but can be changed by the client.  It must be at
  // least 1.
  int m_maxHistorySize;

  // Chosen value, available after exec() returns 1.
  QString m_text;

public:      // funcs
  TextInputDialog(QString const &windowTitle,
                  QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());
  ~TextInputDialog();

  // Set the text that appears in the label above the input control.
  // Initially it is "Input:".
  void setLabelText(QString labelText);

  // Add an item to the front of the history, possibly removing the
  // last entry, depending on 'm_maxHistorySize'.  Also removes any
  // existing item with the same value.
  void rememberInput(QString input);

  // Show the dialog and wait for it to be closed.  Returns 1 on Ok
  // and 0 on Cancel.  Afterward, get the chosen value from 'text'.
  int exec() OVERRIDE;

  // Like 'exec', except if the dialog is already visible, complain and
  // return 0.  Otherwise set the label text to 'prompt' and exec().
  //
  // If 'centerWindow' is not NULL, center the dialog on that window.
  int runPrompt(QString prompt, QWidget *centerWindow = NULL);

  // Like 'runPrompt', except if the entered text is empty, then
  // return 0, and if not, then enter it into the history.
  int runPrompt_nonEmpty(QString prompt, QWidget *centerWindow = NULL);

  // Called by the dialog infrastructure when the user presses Ok.
  void accept() OVERRIDE;
};

#endif // EDITOR_TEXTINPUT_H
