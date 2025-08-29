// host-file-and-line-opt.h
// HostFileAndLineOpt class.

#ifndef EDITOR_HOST_FILE_AND_LINE_OPT_H
#define EDITOR_HOST_FILE_AND_LINE_OPT_H

#include "host-and-resource-name.h"    // HostAndResourceName
#include "line-number.h"               // LineNumber

#include "smbase/sm-macros.h"          // DMEMB

#include <optional>                    // std::optional


// An optional host and file name, and if the name is present, an
// optional line and byte number.
class HostFileAndLineOpt {
public:      // data
  // Host and file name.  Can be empty() to mean none.
  HostAndResourceName m_harn;

  // Optional 1-based line number.
  std::optional<LineNumber> m_line;

  // The 0-based byte index, or -1 to mean "none".
  //
  // TODO: Change this to `std::optional`.
  int m_byteIndex;

public:      // funcs
  HostFileAndLineOpt()
    : m_harn(),
      m_line(),
      m_byteIndex(-1)
  {}

  ~HostFileAndLineOpt()
  {}

  HostFileAndLineOpt(HostFileAndLineOpt const &obj)
    : DMEMB(m_harn),
      DMEMB(m_line),
      DMEMB(m_byteIndex)
  {}

  HostFileAndLineOpt(
    HostAndResourceName const &harn,
    std::optional<LineNumber> line,
    int byteIndex)
    : IMEMBFP(harn),
      IMEMBFP(line),
      IMEMBFP(byteIndex)
  {}

  bool hasFilename() const { return !m_harn.empty(); }
  bool hasLine() const { return m_line.has_value(); }
  bool hasByteIndex() const { return m_byteIndex >= 0; }
};


#endif // EDITOR_HOST_FILE_AND_LINE_OPT_H
