// bufferstate.h
// a Buffer, plus some state suitable for an editor

#ifndef BUFFERSTATE_H
#define BUFFERSTATE_H
                 
#include "buffer.h"     // Buffer
#include "str.h"        // string

class BufferState : public Buffer {
public:      // data
  string filename;           // name of file being edited

public:      // funcs
  BufferState();
  ~BufferState();
};

#endif // BUFFERSTATE_H
