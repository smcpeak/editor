// named-td-editor.cc
// Code for `named-td-editor` module.

#include "named-td-editor.h"           // this module

#include "named-td.h"                  // NamedTextDocument

#include "smbase/sm-file-util.h"       // SMFileUtil


NamedTextDocumentEditor::~NamedTextDocumentEditor()
{}


NamedTextDocumentEditor::NamedTextDocumentEditor(
  NamedTextDocument *d)
  : TextDocumentEditor(d),
    m_namedDoc(d)
{}


string NamedTextDocumentEditor::applyCommandSubstitutions(
  string const &orig) const
{
  // If the document does not have a file name, we will return a pair of
  // quotes, signifying an empty string in the context of a shell
  // command.
  string fname = "''";

  DocumentName const &documentName = m_namedDoc->documentName();

  if (documentName.hasFilename()) {
    SMFileUtil sfu;
    fname = sfu.splitPathBase(documentName.filename());
  }

  // Very simple, naive implementation.
  return replaceAll(orig, "$f", fname);
}


// EOF
