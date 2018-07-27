// diff-hilite.cc
// code for diff-hilite.h

#include "diff-hilite.h"               // this module

// editor
#include "textcategory.h"              // LineCategories

// smbase
#include "array.h"                     // Array


string DiffHighlighter::highlighterName() const
{
  return "Diff";
}


void DiffHighlighter::highlight(TextDocumentCore const &doc, int line,
                                LineCategories &categories)
{
  // Get the line contents.
  int lineLength = doc.lineLengthBytes(line);
  Array<char> lineArray(lineLength);
  doc.getPartialLine(TextMCoord(line, 0), lineArray.ptr(), lineLength);

  // Default category.
  categories.clear(TC_DIFF_CONTEXT);

  if (lineLength == 0) {
    return;
  }

  if (lineLength >= 3) {
    if (lineArray[0] == '-' && lineArray[1] == '-' && lineArray[2] == '-') {
      categories.clear(TC_DIFF_OLD_FILE);
      return;
    }
    if (lineArray[0] == '+' && lineArray[1] == '+' && lineArray[2] == '+') {
      categories.clear(TC_DIFF_NEW_FILE);
      return;
    }
  }

  if (lineLength >= 2 && lineArray[0] == '@' && lineArray[1] == '@') {
    categories.clear(TC_DIFF_SECTION);
    return;
  }

  if (lineArray[0] == '+') {
    categories.clear(TC_DIFF_ADDITION);
  }
  else if (lineArray[0] == '-') {
    categories.clear(TC_DIFF_REMOVAL);
  }
}


// EOF
