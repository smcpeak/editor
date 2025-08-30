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
private:     // data
  // Optional host and file name.  If this is absent, then the whole
  // object effectively represents an absent value.
  std::optional<HostAndResourceName> m_harn;

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
    std::optional<HostAndResourceName> harn,
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

  // Read-only member access.
  std::optional<HostAndResourceName> const &getHarnOpt() const
    { return m_harn; }
  std::optional<LineNumber> const &getLineOpt() const
    { return m_line; }
  std::optional<int> const &getByteIndexOpt() const
    { return m_byteIndex; }

  // Tests for member presence.
  bool hasFilename() const { return m_harn.has_value(); }
  bool hasLine() const { return m_line.has_value(); }
  bool hasByteIndex() const { return m_byteIndex.has_value(); }

  // Requires: hasFilename()
  HostAndResourceName const &getHarn() const;

  // Requires: hasLine()
  LineNumber getLine() const;

  // Requires: hasByteIndex()
  int getByteIndex() const;

  // Set `m_harn` to a present value.
  void setHarn(HostAndResourceName const &harn);
};


#endif // EDITOR_HOST_FILE_AND_LINE_OPT_H
