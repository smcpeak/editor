// nearby-file.h
// Routine to identify a file name within a piece of text.

#ifndef EDITOR_NEARBY_FILE_H
#define EDITOR_NEARBY_FILE_H

// editor
#include "byte-index-fwd.h"                      // ByteIndex [f]
#include "host-and-resource-name-fwd.h"          // HostAndResourceName [f]
#include "host-file-olb-fwd.h"                   // HostFile_OptLineByte [f]
#include "host-file-probe-fwd.h"                 // HostFileProbe [f]

// smbase
#include "smbase/std-optional-fwd.h"             // std::optional [f]
#include "smbase/std-string-fwd.h"               // std::string [f]
#include "smbase/std-vector-fwd.h"               // stdfwd::vector [f]


/* Given a string `haystack` and an offset of a character within that
   string, try to locate the name of an existing file within the
   haystack and containing the character at the given offset.  If the
   offset is out of bounds, this function returns `nullopt`.

   `candidatePrefixes` gives the candidate prefixes (directories) in
   which to look for the file.  It must explicitly include the "current
   directory" if that should be a candidate, and must also explicitly
   include the empty string if absolute paths are to be recognized.

   Note: When a `HostAndResourceName` carries the empty string, it
   returns true to the `empty()` query, but here that is valid.

   Candidates are in priority order, such that if something in the
   haystack looks like a filename, and can be found using two or more of
   the candidate prefixes, whichever prefix appears earlier is selected.

   If no combination of string and prefix yields an existing file, then
   this function returns its best guess about what the intended file
   name string is.  If that is an absolute path, it is returned as-is.
   Otherwise it is returned prefixed with the *first* candidate.

   If no candidate file name string can be found, or no candidate
   prefixes are provided, this returns `nullopt`.
*/
std::optional<HostFile_OptLineByte> getNearbyFilename(
  HostFileProbe &probe,
  stdfwd::vector<HostAndResourceName> const &candidatePrefixes,
  std::string const &haystack,
  ByteIndex charOffset);


#endif // EDITOR_NEARBY_FILE_H
