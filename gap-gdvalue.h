// gap-gdvalue.h
// Conversion between `GapArray` and `GDValue`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_GAP_GDVALUE_H
#define EDITOR_GAP_GDVALUE_H

#include "gap.h"                       // GapArray

#include "smbase/gdvalue.h"            // gdv::GDValue


// Yield as a sequence.
template <typename T>
gdv::GDValue toGDValue(GapArray<T> const &arr)
{
  using namespace gdv;

  GDValue seq(GDVK_SEQUENCE);

  for (int i=0; i < arr.length(); ++i) {
    seq.sequenceAppend(toGDValue(arr.get(i)));
  }

  return seq;
}


// Yield as a single string.
template <>
gdv::GDValue toGDValue(GapArray<char> const &arr);


#endif // EDITOR_GAP_GDVALUE_H
