// macro-creator-dialog.h
// MacroCreatorDialog class.

#ifndef MACRO_CREATOR_DIALOG_H
#define MACRO_CREATOR_DIALOG_H

#include "editor-global.h"             // EditorGlobal, EditorCommandVector
#include "modal-dialog.h"              // ModalDialog

#include "smqtutil/sm-line-edit-fwd.h" // SMLineEdit

class QTextEdit;


// Dialog to show recent commands, and allow the user to create a macro
// out of a certain number of the most recent ones.
class MacroCreatorDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // Global editor data, which is where macros are stored.
  EditorGlobal *m_editorGlobal;

  // When `m_numberOfCommands` is set to a valid value, this is
  // populated with the specified commands.  Otherwise, it is cleared.
  EditorCommandVector m_chosenCommands;

  // ---- controls ----
  // Input specifying how many commands to put into the macro.
  SMLineEdit *m_numberOfCommands;

  // Input speciying the name of the macro to create.
  SMLineEdit *m_macroName;

  // Multi-line read-only editor containing the commands that are ready
  // to be incorporated into a macro.
  QTextEdit *m_commandList;

public:      // funcs
  MacroCreatorDialog(
    EditorGlobal *editorGlobal,
    QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());

  ~MacroCreatorDialog();

  // Read the current text in `m_numberOfCommands` and use that to
  // update `m_commandList` by reading the specific number of recent
  // commands in `m_editorGlobal`.
  void updateCommandList();

  // After getting a true return from `exec()`, call this to get the
  // chosen macro name.  This is not empty (provided `exec` in fact
  // returned true).
  std::string getMacroName() const;

  // Likewise, get the commands that should comprise the macro.  Also
  // not empty.
  EditorCommandVector const &getChosenCommands() const
    { return m_chosenCommands; }

public Q_SLOTS:
  // Invoked when `m_numberOfCommands` changes.
  void on_textEdited(QString const &) NOEXCEPT;

  // Called when "Ok" is pressed.
  virtual void accept() NOEXCEPT OVERRIDE;
};


#endif // MACRO_CREATOR_DIALOG_H
