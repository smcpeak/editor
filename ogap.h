// ogap.h
// OGapArray template class.

#ifndef OGAP_H
#define OGAP_H

// editor
#include "gap.h"                       // GapArray

// smbase
#include "sm-macros.h"                 // NO_OBJECT_COPIES


// Gap array of nullable owner pointers.
template <class T>
class OGapArray {
  NO_OBJECT_COPIES(OGapArray);

private:     // data
  // use a GapArray of void*, so that the compiler will only
  // instantiate the underlying template once; I'll use (inline) casts
  // as necessary in this file to ensure the interface is type-safe
  GapArray<void*> arr;

public:
  OGapArray() : arr() {}        // empty sequence
  ~OGapArray() { clear(); }     // release storage

  // number of elements in sequence
  int length() const              { return arr.length(); }

  // retrieve a serf (non-deletable) pointer from the array
  T *get(int elt)                 { return (T*)arr.get(elt); }
  T const *getC(int elt) const    { return (T const*)arr.get(elt); }

  // replace an existing element with another one; an owner pointer is
  // passed in, and an owner pointer is returned
  T *replace(int elt, T *value)   { return (T*)arr.replace(elt, value); }

  // insert a new element; index becomes 'elt', and later elements
  // have their indices shifted by one
  void insert(int elt, T *value)  { arr.insert(elt, value); }

  // remove an element, returning it as an owner pointer
  T *remove(int elt)              { return (T*)arr.remove(elt); }

  // delete an element directly
  void deleteElt(int elt)         { delete remove(elt); }

  // delete all elements
  void clear();

  // drop gap size to zero
  void squeezeGap()               { arr.squeezeGap(); }

  // debugging
  void getInternals(int &L, int &G, int &R) const
    { arr.getInternals(L, G, R); }
};


template <class T>
void OGapArray<T>::clear()
{
  while (length()) {
    deleteElt(0);
  }
}


#endif // OGAP_H
