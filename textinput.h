// textinput.h
// TextInputDialog class.

#ifndef TEXTINPUT_H
#define TEXTINPUT_H

// editor
#include "modal-dialog.h"              // ModalDialog

// smbase
#include "sm-override.h"               // OVERRIDE

// Qt
#include <QComboBox>
#include <QLabel>
#include <QString>
#include <QStringList>


// Prompt for a single-line text input.  Remembers history of previous
// inputs.
class TextInputDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // Label above the input control.
  QLabel *m_label;

  // The combo box control with editable text and history.
  QComboBox *m_comboBox;

public:      // data
  // History of prior choices, from most to least recent.  This is
  // *not* automatically populated; the must set it, either directly
  // or with 'rememberInput'.  The first entry is also the default
  // value for the input text box.
  QStringList m_history;

  // Used by 'rememberInput', this is maximum number of history
  // elements, after which point the oldest entry is removed.  This is
  // initially 20 but can be changed by the client.  It must be at
  // least 1.
  int m_maxHistorySize;

  // Chosen value, available after exec() returns 1.
  QString m_text;

public:      // funcs
  TextInputDialog(QString windowTitle,
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
  int runPrompt(QString prompt);

  // Like 'runPrompt', except if the entered text is empty, then
  // return 0, and if not, then enter it into the history.
  int runPrompt_nonEmpty(QString prompt);

  // Called by the dialog infrastructure when the user presses Ok.
  void accept() OVERRIDE;
};

#endif // TEXTINPUT_H
