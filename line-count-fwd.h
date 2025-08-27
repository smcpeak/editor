// line-count-fwd.h
// Forward decls for `line-count.h`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LINE_COUNT_FWD_H
#define EDITOR_LINE_COUNT_FWD_H

#include "line-difference-fwd.h"       // LineDifference

// A non-negative `LineDifference`.
//
// My plan is to make a `LineCount` class that only holds non-negative
// values, but for the moment I will just use this alias.
using LineCount = LineDifference;

// A positive `LineDifference`.
using PositiveLineCount = LineDifference;

#endif // EDITOR_LINE_COUNT_FWD_H
