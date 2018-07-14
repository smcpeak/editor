// nearby-file.h
// Routine to identify a file name within a piece of text.

#ifndef NEARBY_FILE_H
#define NEARBY_FILE_H

// smbase
#include "array.h"                     // ArrayStack
#include "macros.h"                    // DMEMB
#include "str.h"                       // string

class SMFileUtil;                      // sm-file-util.h


// An optional file name, and if the name is present, an optional line
// number.
class FileAndLineOpt {
public:      // data
  // The file name, or "" to mean none.
  string m_filename;

  // The line number, or 0 to mean "none".
  int m_line;

public:      // funcs
  FileAndLineOpt()
    : m_filename(""),
      m_line(0)
  {}

  FileAndLineOpt(FileAndLineOpt const &obj)
    : DMEMB(m_filename),
      DMEMB(m_line)
  {}

  FileAndLineOpt(string const &filename, int line)
    : m_filename(filename),
      m_line(line)
  {}

  bool hasFilename() const { return !m_filename.empty(); }
  bool hasLine() const { return m_line != 0; }
};


// Given a string 'haystack' and an offset of a character within that
// string, try to locate the name of an existing file within the
// haystack and containing the character at the given offset.  If the
// offset is out of bounds, this function returns an object for which
// 'hasFilename()' is false.
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
// name string is.  If that is an absolute path, it is returned as-is.
// Otherwise it is returned prefixed with the *first* candidate.
//
// If no candidate file name string can be found, or no candidate
// prefixes are provided, this returns an object whose 'hasFilename()'
// is false.
FileAndLineOpt getNearbyFilename(
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset);


// Algorithmic core, parameterized by the file name probe mechanism
// in order to facilitate testing.
FileAndLineOpt innerGetNearbyFilename(
  SMFileUtil &sfu,
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset);


#endif // NEARBY_FILE_H
