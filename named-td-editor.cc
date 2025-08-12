// named-td-editor.cc
// Code for `named-td-editor` module.

#include "named-td-editor.h"           // this module

#include "named-td.h"                  // NamedTextDocument

#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // shellDoubleQuote, replaceAllMultiple


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
  string fname;
  {
    DocumentName const &documentName = m_namedDoc->documentName();
    if (documentName.hasFilename()) {
      SMFileUtil sfu;
      fname = sfu.splitPathBase(documentName.filename());
    }
  }

  string wordAfter = getWordAfter(cursor());

  // Quote the values for use in a shell context.
  fname = shellDoubleQuote(fname);
  wordAfter = shellDoubleQuote(wordAfter);

  return replaceAllMultiple(orig, {
    {"$f", fname},
    {"$w", wordAfter},
  });
}


// EOF
