// nearby-file.h
// Routine to identify a file name within a piece of text.

#ifndef EDITOR_NEARBY_FILE_H
#define EDITOR_NEARBY_FILE_H

// editor
#include "host-and-resource-name.h"    // HostAndResourceName

// smbase
#include "array.h"                     // ArrayStack
#include "sm-macros.h"                 // DMEMB
#include "str.h"                       // string

class SMFileUtil;                      // sm-file-util.h


// An optional host and file name, and if the name is present, an
// optional line number.
//
// TODO: Move this to its own module.
class HostFileAndLineOpt {
public:      // data
  // Host and file name.  Can be empty() to mean none.
  HostAndResourceName m_harn;

  // The line number, or 0 to mean "none".
  int m_line;

public:      // funcs
  HostFileAndLineOpt()
    : m_harn(),
      m_line(0)
  {}

  ~HostFileAndLineOpt()
  {
    // I'm a little worried about using this type as part of a Qt queued
    // connection signal, so I want to ensure that a destroyed object is
    // easily recognizable in the debugger.
    m_line = -1000;
  }

  HostFileAndLineOpt(HostFileAndLineOpt const &obj)
    : DMEMB(m_harn),
      DMEMB(m_line)
  {}

  HostFileAndLineOpt(HostAndResourceName const &harn, int line)
    : m_harn(harn),
      m_line(line)
  {}

  bool hasFilename() const { return !m_harn.empty(); }
  bool hasLine() const { return m_line != 0; }
};


// Interface with which to test the existence of a host+file.
class IHFExists {
public:      // methods
  // From the client's perspective, invoking this method does not change
  // the state of the receiver object.  However, this method is not
  // 'const' because the main implementor of it, VFS_QuerySync, needs to
  // invoke non-const helper methods due to maintaining state related to
  // the communication used to answer the question.  I'm not sure if
  // there is or should be a better way to handle this situation.
  virtual bool hfExists(HostAndResourceName const &harn) = 0;
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
HostFileAndLineOpt getNearbyFilename(
  IHFExists &ihfExists,
  ArrayStack<HostAndResourceName> const &candidatePrefixes,
  string const &haystack,
  int charOffset);


#endif // EDITOR_NEARBY_FILE_H
