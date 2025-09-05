// host-file-and-line-opt.h
// HostFileAndLineOpt class.

#ifndef EDITOR_HOST_FILE_AND_LINE_OPT_H
#define EDITOR_HOST_FILE_AND_LINE_OPT_H

#include "host-file-and-line-opt-fwd.h"          // fwds for this module

#include "byte-index.h"                          // ByteIndex
#include "host-and-resource-name.h"              // HostAndResourceName
#include "line-index.h"                          // LineIndex

#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue
#include "smbase/sm-macros.h"                    // DMEMB

#include <optional>                              // std::optional


// An optional host and file name, and if the name is present, an
// optional line and byte number.
class HostFileAndLineOpt {
private:     // data
  // Optional host and file name.  If this is absent, then the whole
  // object effectively represents an absent value.
  std::optional<HostAndResourceName> m_harn;

  // Optional 0-based line index.
  std::optional<LineIndex> m_lineIndex;

  // Optional 0-based byte index.
  std::optional<ByteIndex> m_byteIndex;

public:      // funcs
  HostFileAndLineOpt()
    : m_harn(),
      m_lineIndex(),
      m_byteIndex()
  {
    selfCheck();
  }

  ~HostFileAndLineOpt()
  {}

  HostFileAndLineOpt(HostFileAndLineOpt const &obj)
    : DMEMB(m_harn),
      DMEMB(m_lineIndex),
      DMEMB(m_byteIndex)
  {
    selfCheck();
  }

  HostFileAndLineOpt(
    std::optional<HostAndResourceName> harn,
    std::optional<LineIndex> lineIndex,
    std::optional<ByteIndex> byteIndex)
    : IMEMBFP(harn),
      IMEMBFP(lineIndex),
      IMEMBFP(byteIndex)
  {
    selfCheck();
  }

  // Assert invariants.
  void selfCheck() const;

  // Read-only member access.
  std::optional<HostAndResourceName> const &getHarnOpt() const
    { return m_harn; }
  std::optional<LineIndex> const &getLineIndexOpt() const
    { return m_lineIndex; }
  std::optional<ByteIndex> const &getByteIndexOpt() const
    { return m_byteIndex; }

  // Tests for member presence.
  bool hasFilename() const { return m_harn.has_value(); }
  bool hasLineIndex() const { return m_lineIndex.has_value(); }
  bool hasByteIndex() const { return m_byteIndex.has_value(); }

  // Requires: hasFilename()
  HostAndResourceName const &getHarn() const;
  std::string getFilename() const;

  // Requires: hasLine()
  LineIndex getLineIndex() const;

  // Requires: hasByteIndex()
  ByteIndex getByteIndex() const;

  // Set `m_harn` to a present value.
  void setHarn(HostAndResourceName const &harn);

  // Debug dump.
  operator gdv::GDValue() const;
};


#endif // EDITOR_HOST_FILE_AND_LINE_OPT_H
