// hilite.h
// syntax highlighting interface

#ifndef HILITE_H
#define HILITE_H

#include "td-core.h"                   // TextDocumentCore, TextDocumentObserver
#include "td-editor.h"                 // TextDocumentEditor

class LineCategories;                  // textcategory.h


// A highlighter can apply TextCategories to lines of text.  The
// renderer converts those categories to specific colors, etc.
//
// A highlighter is an observer so it can maintain its incremental
// highlighting state.
class Highlighter : public TextDocumentObserver {
public:
  // Clients *are* allowed to delete objects known only as
  // implementors of Highlighter.
  virtual ~Highlighter() {}

  // Name of this highlighter.
  virtual string highlighterName() const = 0;

  // Fill 'categories' with the styles for 'line' in 'doc'.
  //
  // Although 'doc' is a parameter here, a highlighter object is always
  // associated with a specific document object (via a mechanism that
  // depends on the particular implementor class), and 'highlight' must
  // only be passed a reference to that particular object.
  virtual void highlight(TextDocumentCore const &doc, int line,
                         LineCategories &categories) = 0;

  // Convenience method.
  void highlightTDE(TextDocumentEditor const *tde, int line,
                    LineCategories &categories)
    { this->highlight(tde->getDocument()->getCore(), line, categories); }
};


#endif // HILITE_H
