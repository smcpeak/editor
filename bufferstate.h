// bufferstate.h
// a Buffer, plus some state suitable for an editor

#ifndef BUFFERSTATE_H
#define BUFFERSTATE_H
                 
#include "buffer.h"     // Buffer
#include "str.h"        // string

             
class BufferState : public Buffer {
public:      // data         
  // name of file being edited
  string filename;            
                    
  // true when there are unsaved changes
  bool changed;

public:      // funcs
  BufferState();
  ~BufferState();
               
  // empty buffer
  void clear();
};

#endif // BUFFERSTATE_H
