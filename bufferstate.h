// bufferstate.h
// a Buffer, plus some state suitable for an editor

// in an editor, the BufferState would contain all the info that is
// remembered for *undisplayed* buffers

#ifndef BUFFERSTATE_H
#define BUFFERSTATE_H

#include "buffer.h"     // Buffer
#include "str.h"        // string
#include "hilite.h"     // Highlighter


class BufferState : public Buffer {
public:      // data
  // name of file being edited
  string filename;

  // true when there are unsaved changes
  bool changed;

  // current highlighter; clients can come in and replace the
  // highlighter, but it must always be the case that the
  // highlighter is attached to 'this' buffer
  Highlighter *highlighter;      // (nullable owner)

public:      // funcs
  BufferState();
  ~BufferState();
               
  // empty buffer
  void clear();
};

#endif // BUFFERSTATE_H
