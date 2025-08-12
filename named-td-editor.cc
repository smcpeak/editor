// named-td-editor.cc
// Code for `named-td-editor` module.

#include "named-td-editor.h"           // this module

#include "named-td.h"                  // NamedTextDocument


NamedTextDocumentEditor::~NamedTextDocumentEditor()
{}


NamedTextDocumentEditor::NamedTextDocumentEditor(
  NamedTextDocument *d)
  : TextDocumentEditor(d),
    m_namedDoc(d)
{}


// EOF
