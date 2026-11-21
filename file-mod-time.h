// file-mod-time.h
// `getFileModTime` function.

// See license.txt for copyright and terms of use.

/*
  `smbase/nonport.h` defines `getFileModificationTime`, but that has
  several issues:

    * It directly uses `int64_t` as the timestamp type without defining
      an intermediate typedef, making uses of it harder to spot.

    * It has a somewhat strange error reporting interface that, in the
      `editor` repo, I always ignore anyway.

    * It accepts `char const *` instead of `std::string`.

  So, this module is meant to insulate `editor` from those problems.
*/

#ifndef EDITOR_FILE_MOD_TIME_H
#define EDITOR_FILE_MOD_TIME_H

// smbase
#include "smbase/std-string-fwd.h"     // std::string [f]

// libc++
#include <cstdint>                     // std::int64_t


// In `editor`, file time stamps are Unix time with integer granularity.
using FileTimestamp = std::int64_t;

// Get the timestamp of `fname`, or 0 if there is a problem reading it.
FileTimestamp getFileModTime(std::string const &fname);


#endif // EDITOR_FILE_MOD_TIME_H
