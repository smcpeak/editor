// host-file-and-line-opt.h
// HostFileAndLineOpt class.

#ifndef EDITOR_HOST_FILE_AND_LINE_OPT_H
#define EDITOR_HOST_FILE_AND_LINE_OPT_H

// editor
#include "host-and-resource-name.h"    // HostAndResourceName

// smbase
#include "sm-macros.h"                 // DMEMB


// An optional host and file name, and if the name is present, an
// optional line number.
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


#endif // EDITOR_HOST_FILE_AND_LINE_OPT_H
