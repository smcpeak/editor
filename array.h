// array.h
// simple growable array

#ifndef ARRAY_H
#define ARRAY_H

#include "xassert.h"     // xassert
#include <string.h>      // memset

// array of T's, which are bit-copyable, and bit-zero-inited
template <class T>
class Array {
public:      // data
  T *data;           // (owner) array of 'T' objects (nullable?)
  int size;          // number of elements in the array
  int allocated;     // # of elements allocated to 'data'

public:      // funcs
  Array()            // empty array
    : data(NULL), size(0), allocated(0) {}
  ~Array();

  // r/o element access
  T const &elt(int index) const {
    xassert((unsigned)index < (unsigned)size);
    return data[index];
  }

  // r/w element access
  T &operator[] (int index) {
    xassert((unsigned)index < (unsigned)allocated);
    if (index >= size) {
      size = index+1;
    }
    return data[index];
  }

  // change the allocated size, preserving
  // elements in the common prefix
  void realloc(int newAllocd);

  // make the array large enough to hold at least 'n' elements
  void grow(int n)
    { if (n > allocated) { realloc(n); } }

  // like 'grow', except reserve some extra space at
  // the end for additional growth w/o reallocation
  void growWithMargin(int n)
    { if (n > allocated) { realloc(n * 6 / 5 /* 20% */ + 20); } }

  // move some elements
  void move(int destIndex, int srcIndex, int numElts);
};


template <class T>
Array<T>::~Array()
{                         
  if (data) { 
    // call dtors for accessed elements
    for (int i=0; i<size; i++) {
      data[i].T::~T();
    }
    delete [] ((char*)data);
  }
}


template <class T>
void Array<T>::realloc(int newAllocd)
{
  if (newAllocd != allocated) {
    T *newData;
    if (newAllocd > 0) {
      // allocate it as a plain block..
      // opt: only init what won't be subsequently overwritten
      newData = (T*)(new char[newAllocd * sizeof(T)]);

      // .. and bit-zero init it
      memset(newData, 0, newAllocd * sizeof(T));
    }
    else {
      newData = NULL;
    }

    // compute how much original data to save
    int newSize = min(size, newAllocd);

    // transfer that data
    memcpy(newData, data, newSize * sizeof(T));
    
    // now, call destructors on the *unsaved* parts;
    // we assume the pieces beyond 'size' do not need
    // to be destroyed, since presumably they were
    // never accessed
    for (int i=newSize; i<size; i++) {
      data[i].T::~T();
    }

    // reassign all our data fields to the new info
    data = newData;
    size = newSize;
    allocated = newAllocd;
  }
}


template <class T>
void Array<T>::move(int destIndex, int srcIndex, int numElts)
{
  growWithMargin(destIndex + numElts);
  memmove(data+destIndex, data+srcIndex, numElts*sizeof(T));
  if (destIndex + numElts > size) {
    size = destIndex + numElts;
  }
}

#endif // ARRAY_H
