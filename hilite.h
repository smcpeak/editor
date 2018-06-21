// hilite.h
// syntax highlighting interface

#ifndef HILITE_H
#define HILITE_H

#include "buffer.h"                    // BufferCore, BufferObserver

class LineCategories;                  // textcategory.h


class Highlighter : public BufferObserver {
public:
  // clients *are* allowed to delete objects known only as
  // implementors of Highlighter
  virtual ~Highlighter() {}

  // Fill 'categories' with the styles for 'line' in 'buf'.
  virtual void highlight(BufferCore const &buf, int line,
                         LineCategories &categories)=0;
};


#endif // HILITE_H
