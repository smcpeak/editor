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
  //
  // TODO: Use std::optional for this one too.
  HostAndResourceName m_harn;

  // Optional 1-based line number.
  std::optional<LineNumber> m_line;

  // Optional 0-based byte index.
  std::optional<int> m_byteIndex;

public:      // funcs
  HostFileAndLineOpt()
    : m_harn(),
      m_line(),
      m_byteIndex()
  {
    selfCheck();
  }

  ~HostFileAndLineOpt()
  {}

  HostFileAndLineOpt(HostFileAndLineOpt const &obj)
    : DMEMB(m_harn),
      DMEMB(m_line),
      DMEMB(m_byteIndex)
  {
    selfCheck();
  }

  HostFileAndLineOpt(
    HostAndResourceName const &harn,
    std::optional<LineNumber> line,
    std::optional<int> byteIndex)
    : IMEMBFP(harn),
      IMEMBFP(line),
      IMEMBFP(byteIndex)
  {
    selfCheck();
  }

  // Assert invariants.
  void selfCheck() const;

  bool hasFilename() const { return !m_harn.empty(); }
  bool hasLine() const { return m_line.has_value(); }
  bool hasByteIndex() const { return m_byteIndex.has_value(); }
};


#endif // EDITOR_HOST_FILE_AND_LINE_OPT_H
