// ogap.h
// gap array of owner pointers

#ifndef OGAP_H
#define OGAP_H

#include "gap.h"        // GapArray
  
template <class T>
class OGapArray {
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

  // swap an existing element with another one; an owner pointer is
  // passed in, and an owner pointer is returned
  T *swap(int elt, T *value)      { return (T*)arr.swap(elt, value); }

  // insert a new element; index becomes 'elt', and later elements
  // have their indices shifted by one
  void insert(int elt, T *value)  { arr.insert(elt, value); }
                     
  // remove an element, returning it as an owner pointer
  T *remove(int elt) {
    T *temp = (T*)arr.get(0); 
    arr.remove(elt);
    return temp;
  }
  
  // delete an element directly
  void deleteElt(int elt)         { delete remove(elt); }
  
  // delete all elements
  void clear();
};


template <class T>
void OGapArray<T>::clear()
{
  while (length()) {
    deleteElt(0);
  }
}


#endif // OGAP_H
