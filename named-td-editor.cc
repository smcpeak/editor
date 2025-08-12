// named-td-editor.cc
// Code for `named-td-editor` module.

#include "named-td-editor.h"           // this module

#include "named-td.h"                  // NamedTextDocument

#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // shellDoubleQuote, replaceAllMultiple, splitMultiSep
#include "smbase/stringb.h"            // stringb

#include <utility>                     // std::pair
#include <vector>                      // std::vector


NamedTextDocumentEditor::~NamedTextDocumentEditor()
{}


NamedTextDocumentEditor::NamedTextDocumentEditor(
  NamedTextDocument *d)
  : TextDocumentEditor(d),
    m_namedDoc(d)
{}


std::string NamedTextDocumentEditor::applyCommandSubstitutions(
  std::string const &orig) const
{
  std::vector<std::pair<std::string, std::string>> replacements;

  // $f: File name.
  {
    std::string fname;
    DocumentName const &documentName = m_namedDoc->documentName();
    if (documentName.hasFilename()) {
      SMFileUtil sfu;
      fname = sfu.splitPathBase(documentName.filename());
    }
    replacements.push_back({"$f", shellDoubleQuote(fname)});
  }

  // $w: Word at cursor.
  {
    string wordAfter = getWordAfter(cursor());
    replacements.push_back({"$w", shellDoubleQuote(wordAfter)});
  }

  // $t1 .. $t9: Whitespace-separated tokens on cursor line.
  {
    std::string lineText = getWholeLineString(cursor().m_line);
    std::vector<std::string> tokens =
      splitMultiSep(lineText, " \t", false /*wantEmpty*/);
    for (std::size_t i=0; i<9; ++i) {
      std::string var = stringb("$t" << (i+1));
      std::string word =
        (i < tokens.size()? tokens.at(i) : std::string(""));
      replacements.push_back({var, shellDoubleQuote(word)});
    }
  }

  return replaceAllMultiple(orig, replacements);
}


// EOF
