// host-file-and-line-opt.h
// HostFileAndLineOpt class.

#ifndef EDITOR_HOST_FILE_AND_LINE_OPT_H
#define EDITOR_HOST_FILE_AND_LINE_OPT_H

// editor
#include "host-and-resource-name.h"    // HostAndResourceName

// smbase
#include "smbase/sm-macros.h"          // DMEMB


// An optional host and file name, and if the name is present, an
// optional line and byte number.
class HostFileAndLineOpt {
public:      // data
  // Host and file name.  Can be empty() to mean none.
  HostAndResourceName m_harn;

  // The 1-based line number, or 0 to mean "none".
  int m_line;

  // The 0-based byte index, or -1 to mean "none".
  int m_byteIndex;

public:      // funcs
  HostFileAndLineOpt()
    : m_harn(),
      m_line(0),
      m_byteIndex(-1)
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
      DMEMB(m_line),
      DMEMB(m_byteIndex)
  {}

  HostFileAndLineOpt(
    HostAndResourceName const &harn,
    int line,
    int byteIndex)
    : IMEMBFP(harn),
      IMEMBFP(line),
      IMEMBFP(byteIndex)
  {}

  bool hasFilename() const { return !m_harn.empty(); }
  bool hasLine() const { return m_line != 0; }
  bool hasByteIndex() const { return m_byteIndex >= 0; }
};


#endif // EDITOR_HOST_FILE_AND_LINE_OPT_H
