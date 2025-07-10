// apply-command-dialog.h
// ApplyCommandDialog class.

#ifndef EDITOR_APPLY_COMMAND_DIALOG_H
#define EDITOR_APPLY_COMMAND_DIALOG_H

#include "editor-widget-fwd.h"         // EditorWidget
#include "modal-dialog.h"              // ModalDialog

#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/sm-noexcept.h"        // NOEXCEPT
#include "smbase/sm-override.h"        // OVERRIDE

#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QToolButton;


// Prompt the user for a command to run for the Apply command.
//
// See doc/apply-cmd-dialog.html for detailed behavioral specification.
class ApplyCommandDialog : public ModalDialog {
  Q_OBJECT
  NO_OBJECT_COPIES(ApplyCommandDialog);

private:     // data
  // --------------------------- editor data ---------------------------
  // The widget the user was interacting with when they spawned this
  // dialog.  This gives us access to the document name and directory,
  // as well as `EditorGlobal`.
  EditorWidget *m_editorWidget;

  // ---------------------------- controls -----------------------------
  // "Command to run in $PWD."
  QLabel *m_pwdLabel = nullptr;

  // 'Run a previous command (if "New" is empty):'.
  QLabel *m_prevCommandsLabel = nullptr;

  // List of previously executed commands.
  QListWidget *m_prevCommandsListWidget = nullptr;

  // "Copy to New".
  QPushButton *m_copyButton = nullptr;

  // "Delete".
  QPushButton *m_deleteButton = nullptr;

  // "Run a new command (if not empty):".
  QLabel *m_newCommandLabel = nullptr;

  // Text of a new command.
  QLineEdit *m_newCommandLineEdit = nullptr;

  // "X".
  QToolButton *m_clearNewCommandButton = nullptr;

  // "Enable substitution (see help)"
  QCheckBox *m_enableSubstitutionCheckBox = nullptr;

private Q_SLOTS:
  // Copy the selected item in `m_prevCommandsListWidget` into
  // `m_newCommandLineEdit`.
  void copyToNew() NOEXCEPT;

  // Delete the selected item from `m_prevCommandsListWidget`, and also
  // remove it from the editor global settings.
  void deleteSelected() NOEXCEPT;

  // Clear `m_newCommandLineEdit`.
  void clearNewCommand() NOEXCEPT;

public:      // funcs
  ApplyCommandDialog(EditorWidget *editorWidget);
  ~ApplyCommandDialog();

  // After `exec()` returns true, get the command the user chose.
  std::string getSpecifiedCommand() const;

  // After `exec()` returns true, true if the substitution checkbox is
  // enabled.
  bool isSubstitutionEnabled() const;

public Q_SLOTS:
  // Close the dialog and run the specified command, if there is one.
  // If there is no specified command, then do nothing, leaving the
  // dialog open.
  virtual void accept() NOEXCEPT OVERRIDE;
};


#endif // EDITOR_APPLY_COMMAND_DIALOG_H
