// hilite.h
// syntax highlighting interface

#ifndef HILITE_H
#define HILITE_H

#include "style.h"         // LineStyle

class Highlighter {
public:
  virtual ~Highlighter() {}     // silence warning

  // fill 'style' with the styles for 'line' in 'buf'
  virtual void highlight(Buffer const &buf, int line, LineStyle &style);
};


#endif // HILITE_H
