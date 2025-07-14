// fonts-dialog.h
// FontsDialog class.

#ifndef FONTS_DIALOG_H
#define FONTS_DIALOG_H

// editor
#include "builtin-font.h"              // BuiltinFont
#include "editor-global-fwd.h"         // EditorGlobal
#include "modal-dialog.h"              // ModalDialog

// smbase
#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/sm-override.h"        // OVERRIDE


class QComboBox;


// Dialog to let the user set font preferences.
//
// Currently it only offers a single choice, so "fonts" is not quite an
// accurate name, but perhaps in the future I'll expand it.
class FontsDialog : public ModalDialog {
  Q_OBJECT

private:      // data
  // Global editor data, which this dialog customizes.
  RCSerf<EditorGlobal> m_editorGlobal;

  // Dropdown for choosing one of the built-in fonts.
  QComboBox *m_builtinFontDropdown;

public:       // funcs
  FontsDialog(QWidget *parent,
              EditorGlobal *editorGlobal);
  ~FontsDialog();

  // QDialog slots.
  virtual void accept() NOEXCEPT OVERRIDE;
};


#endif // FONTS_DIALOG_H
