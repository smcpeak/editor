// hilite.h
// syntax highlighting interface

#ifndef HILITE_H
#define HILITE_H

#include "buffer.h"        // BufferCore, BufferObserver

class LineStyle;           // style.h


class Highlighter : public BufferObserver {
public:                             
  // clients *are* allowed to delete objects known only as
  // implementors of Highlighter
  virtual ~Highlighter() {}

  // fill 'style' with the styles for 'line' in 'buf'
  virtual void highlight(BufferCore const &buf, int line, LineStyle &style)=0;
};


#endif // HILITE_H
