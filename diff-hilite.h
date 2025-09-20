// diff-hitlite.h
// DiffHighlighter class.

#ifndef DIFF_HILITE_H
#define DIFF_HILITE_H

// editor
#include "hilite.h"                    // Highlighter

// smbase
#include "smbase/sm-override.h"        // OVERRIDE


// A highlighter for diff output.
class DiffHighlighter : public Highlighter {
public:
  // Highlighter methods.
  virtual string highlighterName() const OVERRIDE;
  virtual void highlight(TextDocumentCore const &doc, LineIndex line,
                         LineCategoryAOAs &categories) OVERRIDE;
};


#endif // DIFF_HILITE_H
