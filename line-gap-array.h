// line-gap-array.h
// `LineGapArray`, a `GapArray` specialized to use `LineIndex`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_GAP_ARRAY_H
#define EDITOR_LINE_GAP_ARRAY_H

#include "spec-gap-array.h"            // SpecializedGapArray
#include "line-count.h"                // LineCount
#include "line-index.h"                // LineIndex

#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES


// A `GapArray` specialized to use `LineIndex` and `LineCount`.
template <typename T>
class LineGapArray
                 : public SpecializedGapArray<T, LineIndex, LineCount> {
  NO_OBJECT_COPIES(LineGapArray);

public:      // types
  using Base = SpecializedGapArray<T, LineIndex, LineCount>;

public:      // methods
  using Base::Base;
};


#endif // EDITOR_LINE_GAP_ARRAY_H
