// diff-hilite.cc
// code for diff-hilite.h

#include "diff-hilite.h"               // this module

// editor
#include "textcategory.h"              // LineCategoryAOAs

// smbase
#include "smbase/array.h"              // ArrayStack


string DiffHighlighter::highlighterName() const
{
  return "Diff";
}


void DiffHighlighter::highlight(TextDocumentCore const &doc, LineIndex line,
                                LineCategoryAOAs &categories)
{
  // Get the line contents.
  ByteCount lineLength = doc.lineLengthBytes(line);
  ArrayStack<char> lineArray(lineLength.get());
  doc.getPartialLine(TextMCoord(line, ByteIndex(0)), lineArray, lineLength);

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
