// fonts-dialog.cc
// code for fonts-dialog.h

#include "fonts-dialog.h"              // this module

// editor
#include "editor-global.h"             // EditorGlobal

// smqtutil
#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME

// smbase
#include "smbase/xassert.h"            // xassert

// Qt
#include <QComboBox>
#include <QMessageBox>
#include <QVBoxLayout>


FontsDialog::FontsDialog(
  QWidget *parent,
  EditorGlobal *editorGlobal)
  : ModalDialog(parent),
    m_editorGlobal(editorGlobal),
    m_builtinFontDropdown(NULL)
{
  this->setObjectName("FontsDialog");
  this->setWindowTitle("Editor Font");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  m_builtinFontDropdown = new QComboBox();
  SET_QOBJECT_NAME(m_builtinFontDropdown);
  FOR_EACH_BUILTIN_FONT(bfont) {
    m_builtinFontDropdown->addItem(builtinFontName(bfont));
  }
  m_builtinFontDropdown->setCurrentIndex(
    m_editorGlobal->getEditorBuiltinFont());
  vbox->addWidget(m_builtinFontDropdown);

  createOkAndCancelHBox(vbox);
}


FontsDialog::~FontsDialog()
{}


void FontsDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  int idx = m_builtinFontDropdown->currentIndex();
  if (idx < 0 || idx >= NUM_BUILTIN_FONTS) {
    messageBox(this, "Error",
      "The builtin-font dropdown does not have a valid selection.");
    return;
  }

  m_editorGlobal->setEditorBuiltinFont(
    static_cast<BuiltinFont>(idx));

  ModalDialog::accept();

  GENERIC_CATCH_END
}


// EOF
