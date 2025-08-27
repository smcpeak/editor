// line-gap-array.h
// `LineGapArray`, a `GapArray` specialized to use `LineIndex`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_GAP_ARRAY_H
#define EDITOR_LINE_GAP_ARRAY_H

#include "gap.h"                       // GapArray
#include "line-count.h"                // LineCount
#include "line-index.h"                // LineIndex

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES, EMEMB


template <typename T>
class LineGapArray {
  NO_OBJECT_COPIES(LineGapArray);

private:     // data
  // Underlying array.
  GapArray<T> m_arr;

public:      // methods
  LineGapArray()
    : m_arr()
  {}

  ~LineGapArray()
  {}

  bool operator==(LineGapArray const &obj) const
    { return EMEMB(m_arr); }

  bool operator!=(LineGapArray const &obj) const
    { return !operator==(obj); }

  LineCount length() const
    { return LineCount(m_arr.length()); }

  T const &get(LineIndex elt) const
    { return m_arr.get(elt.get()); }

  void set(LineIndex elt, T const &value)
    { m_arr.set(elt.get(), value); }

  T replace(LineIndex elt, T const &value)
    { return m_arr.replace(elt.get(), value); }

  void insert(LineIndex elt, T const &value)
    { m_arr.insert(elt.get(), value); }

  void insertMany(LineIndex elt, T const *src, LineCount srcLen)
    { m_arr.insertMany(elt.get(), src, srcLen.get()); }

  void insertManyZeroes(LineIndex elt, LineCount insLen)
    { m_arr.insertManyZeroes(elt.get(), insLen.get()); }

  T remove(LineIndex elt)
    { return m_arr.remove(elt.get()); }

  void removeMany(LineIndex elt, LineCount numElts)
    { m_arr.removeMany(elt.get(), numElts.get()); }

  void clear()
    { m_arr.clear(); }

  void swapWith(LineGapArray<T> &other) NOEXCEPT
    { m_arr.swapWith(other.m_arr); }

  void squeezeGap()
    { m_arr.squeezeGap(); }

  void fillFromArray(
    T const *src,
    LineCount srcLen,
    LineIndex elt = LineIndex(0),
    int gapSize=10)
  {
    m_arr.fillFromArray(src, srcLen.get(), elt.get(), gapSize);
  }

  void writeIntoArray(
    T *dest,
    LineCount destLen,
    LineIndex elt = LineIndex(0)) const
  {
    m_arr.writeIntoArray(dest, destLen.get(), elt.get());
  }

  void ensureValidIndex(LineIndex index)
    { m_arr.ensureValidIndex(index.get()); }

  void getInternals(int &L, int &G, int &R) const
    { m_arr.getInternals(L, G, R); }

  // Defined in gap-gdvalue.h.
  inline operator gdv::GDValue() const;
};


#endif // EDITOR_LINE_GAP_ARRAY_H
