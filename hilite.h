// hilite.h
// syntax highlighting interface

#ifndef HILITE_H
#define HILITE_H

class LineStyle;           // style.h
class BufferCore;          // buffer.h

class Highlighter {
public:
  virtual ~Highlighter() {}     // silence warning

  // fill 'style' with the styles for 'line' in 'buf'
  virtual void highlight(BufferCore const &buf, int line, LineStyle &style)=0;
};


#endif // HILITE_H
