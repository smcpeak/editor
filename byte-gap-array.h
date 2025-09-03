// byte-gap-array.h
// `ByteGapArray`, an array of bytes implemented as a "gap" array.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_BYTE_GAP_ARRAY_H
#define EDITOR_BYTE_GAP_ARRAY_H

#include "spec-gap-array.h"            // SpecializedGapArray
#include "byte-count.h"                // ByteCount
#include "byte-index.h"                // ByteIndex

#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES


// A `GapArray` specialized to use `ByteIndex` and `ByteCount`.
template <typename T>
class ByteGapArray
                 : public SpecializedGapArray<T, ByteIndex, ByteCount> {
  NO_OBJECT_COPIES(ByteGapArray);

public:      // types
  using Base = SpecializedGapArray<T, ByteIndex, ByteCount>;

public:      // methods
  using Base::Base;
};

#endif // EDITOR_BYTE_GAP_ARRAY_H
