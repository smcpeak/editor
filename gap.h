// gaparray.h
// growable array with a gap for efficient insertion

#ifndef GAPARRAY_H
#define GAPARRAY_H

#include <string.h>     // memcpy, etc.
//#include <stdio.h>      // printf, for debugging

#include "xassert.h"    // xassert

// abstractly models a sequence
// assumption: objects of type T can be copied with memcpy, etc.,
// and do not have (nontrivial) constructors or destructors
template <class T>
class GapArray {
private:     // data
  // base array
  T *array;

  // number of elements in the first part of the array
  int left;

  // number of spaces in the gap between left and right
  int gap;

  // number of elements in the second part of the array
  int right;

  // invariants:
  //   - all the integers are >= 0
  //   - # of allocated slots in 'array' equals allocated()
  //   - allocated()==0 iff array==NULL

private:     // funcs
  // allocated size (number of elements) of 'array'
  int allocated() const { return left+gap+right; }

  // bounds check
  void bc(int elt) const { xassert(0 <= elt && elt < left+right); }

  // element reference
  T &eltRef(int elt) const;

  // move/widen the gap; postcondition:
  //   - left == elt
  //   - gap >= gapSize
  void makeGapAt(int elt, int gapSize=1);

  // stuff common to the insert() routines
  void prepareToInsert(int elt, int insLen);

public:      // funcs
  GapArray();      // empty sequence
  ~GapArray();     // release storage

  // number of elements in sequence
  int length() const                { return left+right; }

  // get/set an element of the sequence; 'elt' must be
  // between 0 and length()-1
  T get(int elt) const              { return eltRef(elt); }
  void set(int elt, T value)        { eltRef(elt) = value; }

  // set an element, yielding the old value as the return value
  T swap(int elt, T value);

  // insert an element; index becomes 'elt' and all elements with
  // original index 'elt' or greater are shifted up one
  void insert(int elt, T value);

  // insert a sequence of elements at 'elt'; equivalent to
  //   while (srcLen--) { insert(elt++, *src++); }
  void insertMany(int elt, T const *src, int srcLen);

  // use memset(0) to clear 'insLen' elements at 'elt'; equivalent to
  //   while (insLen--) { insert(elt++, 0); }
  void insertManyZeroes(int elt, int insLen);

  // remove an element; all elements with original index 'elt' or
  // greater are shifted down one
  void remove(int elt);

  // remove many elements; equivalent to
  //   while (numElts--) { remove(elt); }
  void removeMany(int elt, int numElts);

  // remove all elements
  void clear();

  // fill from a source array, putting the gap of size at least
  // 'gapSize' at 'elt'; this clears the sequence before filling
  void fillFromArray(T const *src, int srcLen, int elt=0, int gapSize=10);

  // write 'destLen' elements into 'dest', which must be at least that
  // big; first source element written is 'elt' (first destination
  // element is always dest[0], *not* dest[elt])
  void writeIntoArray(T *dest, int destLen, int elt=0) const;
  
  // debugging
  void getInternals(int &L, int &G, int &R) const
    { L=left; G=gap; R=right; }
};


template <class T>
GapArray<T>::GapArray()
  : array(NULL),
    left(0),
    gap(0),
    right(0)
{}

template <class T>
GapArray<T>::~GapArray()
{
  if (array) {
    delete[] array;
  }
}


template <class T>
T &GapArray<T>::eltRef(int elt) const
{
  bc(elt);

  if (elt < left) {
    return array[elt];
  }
  else {
    return array[elt+gap];
  }
}


template <class T>
void GapArray<T>::prepareToInsert(int elt, int insLen)
{
  xassert(0 <= elt && elt <= length());
  if (elt != left ||
      gap < insLen) {
    makeGapAt(elt, insLen);
  }
}

  
template <class T>
T GapArray<T>::swap(int elt, T value)
{
  T ret = get(elt);
  set(elt, value);
  return ret;
}


template <class T>
void GapArray<T>::insert(int elt, T value)
{
  prepareToInsert(elt, 1 /*insLen*/);

  // add at left edge of the gap
  array[left] = value;
  left++;
  gap--;
}


template <class T>
void GapArray<T>::insertMany(int elt, T const *src, int srcLen)
{
  prepareToInsert(elt, srcLen);

  // copy elements into the left edge of the gap
  memcpy(array+left /*dest*/, src, srcLen * sizeof(T));
  left += srcLen;
  gap -= srcLen;
}


template <class T>
void GapArray<T>::insertManyZeroes(int elt, int insLen)
{
  prepareToInsert(elt, insLen);

  // zero the elements at the left edge of the gap
  memset(array+left /*dest*/, 0, insLen * sizeof(T));
  left += insLen;
  gap -= insLen;
}


template <class T>
void GapArray<T>::remove(int elt)
{
  bc(elt);
  if (elt != left) {
    makeGapAt(elt);
  }

  // remove at left edge of right half
  gap++;
  right--;
}


template <class T>
void GapArray<T>::removeMany(int elt, int numElts)
{
  xassert(numElts >= 0);
  xassert(0 <= elt && elt <= left+right-numElts);

  if (elt != left) {
    makeGapAt(elt);
  }

  // remove from left edge of right half
  gap += numElts;
  right -= numElts;
}


template <class T>
void GapArray<T>::makeGapAt(int elt, int gapSize)
{
  // must move gap?
  if (elt != left) {
    if (elt < left) {

      //  <----- left -----><-- gap --><--- right --->
      //  [----------------][---------][-------------]
      //  <-- elt --><-amt->        ^
      //                |           |
      //                +-----------+

      // # of elements to move
      int amt = left-elt;

      memmove(array + left+gap-amt,           // dest
              array + elt,                    // src
              amt * sizeof(T));               // size

      // update stats
      left -= amt;
      right += amt;
    }

    else /* elt > left */ {

      //  <--- left ---><-- gap --><----- right ----->
      //  [------------][---------][-----------------]
      //  <---------- elt+gap ----------->
      //                  ^        <-amt->
      //                  |           |
      //                  +-----------+

      // # of elements to move
      int amt = elt-left;     // elt+gap - (left+gap)

      memmove(array + left,                   // dest
              array + left+gap,               // src (elt+gap - amt)
              amt * sizeof(T));               // size

      // update stats
      left += amt;
      right -= amt;
    }
  }
  xassert(elt == left);

  // must widen gap?
  if (gap < gapSize) {                                     
    // new array size: 150% of existing array size, plus 10
    int newSize = (left+gap+right) * 3 / 2 + 10;
    int newGap = newSize-left-right;
    if (newGap < gapSize) {
      newGap = gapSize;     // or desired size, if that's bigger
    }

    //printf("expanding from %d to %d (eltsize=%d, gapSize=%d)\n",
    //       left+gap+right,
    //       left+newGap+right,
    //       sizeof(T), gapSize);

    // allocate some new space
    T *newArray = new T[left+newGap+right];

    // array:
    //   <--- left ---><-- gap --><----- right ----->
    //   [------------][---------][-----------------]
    // newArray:
    //   <--- left ---><-- newGap --><----- right ----->
    //   [------------][------------][-----------------]

    // fill the halves
    memcpy(newArray /*dest*/, array /*src*/, left * sizeof(T));
    memcpy(newArray+left+newGap /*dest*/, array+left+gap /*src*/, right * sizeof(T));

    // throw away the old space, replace with new
    if (array) {
      delete[] array;
    }
    array = newArray;
    gap = newGap;
  }
  xassert(gap >= gapSize);
}


template <class T>
void GapArray<T>::clear()
{
  gap += left+right;
  left = 0;
  right = 0;
}


template <class T>
void GapArray<T>::fillFromArray(T const *src, int srcLen, int elt, int gapSize)
{
  xassert(srcLen >= 0);
  xassert(0 <= elt && elt <= srcLen);
  xassert(gapSize >= 0);

  // move all available space into the gap
  clear();

  // need a bigger array?
  if (gap < srcLen+gapSize) {
    if (array) {
      delete[] array;
    }

    // don't try to accomodate future growth; if it's needed, it can
    // use the normal mechanism
    gap = srcLen+gapSize;
    array = new T[gap];
  }

  // set desired partition sizes
  left = elt;
  right = srcLen-elt;
  gap -= left+right;
  xassert(gap >= gapSize);

  // src:
  //   <--------- srcLen -------------->
  //   <--- left ---><----- right ----->
  //   [------------][-----------------]
  // array:
  //   <--- left ---><-- gap --><----- right ----->
  //   [------------][---------][-----------------]

  // fill the halves
  memcpy(array /*dest*/, src /*src*/, left * sizeof(T));
  memcpy(array+left+gap /*dest*/, src+left /*src*/, right * sizeof(T));
}

    //   <-- elt --><-- destLen -->
    //   <--- left ---><----- right ------->
    //   [----------**][***********--------]

template <class T>
void GapArray<T>::writeIntoArray(T *dest, int destLen, int elt) const
{
  xassert(destLen >= 0);
  xassert(0 <= elt && elt+destLen <= length());

  if (elt < left) {
    // array:
    //   <-- elt --><-------- destLen+gap ----->
    //   <--- left -----><-- gap --><----- right ------->
    //   [----------****][---------][***********--------]
    //              <amt>           <-- amt2 -->
    // dest:
    //   <--- destLen --->
    //   ****][***********
    //   <amt><-- amt2 -->

    int amt = min(left-elt, destLen);
    memcpy(dest,                     // dest
           array+elt,                // src
           amt * sizeof(T));         // size

    int amt2 = destLen-amt;
    memcpy(dest+amt,                 // dest
           array+left+gap,           // src
           amt2 * sizeof(T));        // size
  }

  else /* elt >= left */ {
    // array:
    //   <-------- elt+gap ---------><-- destLen -->
    //   <--- left ---><-- gap --><----- right ------->
    //   [------------][---------][--***************--]
    // dest:
    //   <-- destLen -->
    //   ***************

    memcpy(dest,                     // dest
           array+elt+gap,            // src
           destLen * sizeof(T));     // size
  }
}

#endif // GAPARRAY_H
