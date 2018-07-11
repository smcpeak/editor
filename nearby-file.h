// nearby-file.h
// Routine to identify a file name within a piece of text.

#ifndef NEARBY_FILE_H
#define NEARBY_FILE_H

#include "array.h"                     // ArrayStack
#include "str.h"                       // string

class SMFileUtil;                      // sm-file-util.h


// Given a string 'haystack' and an offset of a character within that
// string, try to locate the name of an existing file within the
// haystack and caontaining the character at the given offset.  If the
// offset is out of bounds, this function returns the empty string.
//
// 'candidatePrefixes' gives the candidate prefixes (directories) in
// which to look for the file.  It must explicitly include the "current
// directory" if that should be a candidate, and must also explicitly
// include the empty string if absolute paths are to be recognized.
//
// Candidates are in priority order, such that if something in the
// haystack looks like a filename, and can be found using two or more of
// the candidate prefixes, whichever prefix appears earlier is selected.
//
// If no combination of string and prefix yields an existing file, then
// this function returns its best guess about what the intended file
// name string is, prefixed with the *first* candidate.
//
// If no candidate file name string can be found, or no candidate
// prefixes are provided, this returns the empty string.
string getNearbyFilename(
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset);


// Algorithmic core, parameterized by the file name probe mechanism
// in order to facilitate testing.
string innerGetNearbyFilename(
  SMFileUtil &sfu,
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset);


#endif // NEARBY_FILE_H
