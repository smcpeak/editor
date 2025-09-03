// spec-gap-array.h
// `SpecializedGapArray`, a `GapArray` specialized by index/count types.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_SPEC_GAP_ARRAY_H
#define EDITOR_SPEC_GAP_ARRAY_H

#include "gap.h"                       // GapArray

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES, EMEMB


// An array of `Elem`, indexed with `ElemIndex`, and counted with
// `ElemCount`.
template <typename Elem, typename ElemIndex, typename ElemCount>
class SpecializedGapArray {
  NO_OBJECT_COPIES(SpecializedGapArray);

private:     // data
  // Underlying array.
  GapArray<Elem> m_arr;

public:      // methods
  SpecializedGapArray()
    : m_arr()
  {}

  ~SpecializedGapArray()
  {}

  bool operator==(SpecializedGapArray const &obj) const
    { return EMEMB(m_arr); }

  bool operator!=(SpecializedGapArray const &obj) const
    { return !operator==(obj); }

  ElemCount length() const
    { return ElemCount(m_arr.length()); }

  Elem const &get(ElemIndex elt) const
    { return m_arr.get(elt.get()); }

  void set(ElemIndex elt, Elem const &value)
    { m_arr.set(elt.get(), value); }

  Elem replace(ElemIndex elt, Elem const &value)
    { return m_arr.replace(elt.get(), value); }

  void insert(ElemIndex elt, Elem const &value)
    { m_arr.insert(elt.get(), value); }

  void insertMany(ElemIndex elt, Elem const *src, ElemCount srcLen)
    { m_arr.insertMany(elt.get(), src, srcLen.get()); }

  void insertManyZeroes(ElemIndex elt, ElemCount insLen)
    { m_arr.insertManyZeroes(elt.get(), insLen.get()); }

  Elem remove(ElemIndex elt)
    { return m_arr.remove(elt.get()); }

  void removeMany(ElemIndex elt, ElemCount numElts)
    { m_arr.removeMany(elt.get(), numElts.get()); }

  void clear()
    { m_arr.clear(); }

  void swapWith(SpecializedGapArray &other) NOEXCEPT
    { m_arr.swapWith(other.m_arr); }

  void squeezeGap()
    { m_arr.squeezeGap(); }

  void fillFromArray(
    Elem const *src,
    ElemCount srcLen,
    ElemIndex elt = ElemIndex(0),
    ElemCount gapSize = ElemCount(10))
  {
    m_arr.fillFromArray(src, srcLen.get(), elt.get(), gapSize.get());
  }

  void writeIntoArray(
    Elem *dest,
    ElemCount destLen,
    ElemIndex elt = ElemIndex(0)) const
  {
    m_arr.writeIntoArray(dest, destLen.get(), elt.get());
  }

  void ensureValidIndex(ElemIndex index)
    { m_arr.ensureValidIndex(index.get()); }

  void getInternals(int &L, int &G, int &R) const
    { m_arr.getInternals(L, G, R); }

  // Defined in gap-gdvalue.h.
  inline operator gdv::GDValue() const;
};


#endif // EDITOR_SPEC_GAP_ARRAY_H
