// macro-run-dialog.h
// MacroRunDialog class.

#ifndef MACRO_RUN_DIALOG_H
#define MACRO_RUN_DIALOG_H

#include "editor-global-fwd.h"         // EditorGlobal
#include "modal-dialog.h"              // ModalDialog

#include "smbase/sm-override.h"        // OVERRIDE
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <Qt>                          // Qt::WindowFlags

#include <string>                      // std::string

class QListWidget;
class QWidget;


// Dialog to show the list of macros and allow choosing one to run.
//
// In the future it might offer more management options such as deleting,
// renaming, or binding keys.
//
class MacroRunDialog : public ModalDialog {
  Q_OBJECT

private:     // data
  // Global editor data, which is where macros are stored.
  EditorGlobal *m_editorGlobal;

  // Set upon successful `accept()`.
  std::string m_chosenMacroName;

  // ---- controls ----
  // List of all defined macros.
  QListWidget *m_macroList;

public:      // funcs
  MacroRunDialog(
    EditorGlobal *editorGlobal,
    QWidget *parent = NULL, Qt::WindowFlags f = Qt::WindowFlags());

  ~MacroRunDialog();

  // After `exec()` returns true, call this to get the name of the
  // chosen macro to run.
  std::string const &getMacroName() const
    { return m_chosenMacroName; }

public Q_SLOTS:
  // Called when "Ok" is pressed.
  virtual void accept() NOEXCEPT OVERRIDE;
};


#endif // MACRO_RUN_DIALOG_H
