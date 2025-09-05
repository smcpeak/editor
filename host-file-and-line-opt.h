// host-file-and-line-opt.h
// HostFile_OptLineByte class.

// TODO: Rename this file to `host-file-olb.h`.

#ifndef EDITOR_HOST_FILE_AND_LINE_OPT_H
#define EDITOR_HOST_FILE_AND_LINE_OPT_H

#include "host-file-and-line-opt-fwd.h"          // fwds for this module

#include "byte-index.h"                          // ByteIndex
#include "host-and-resource-name.h"              // HostAndResourceName
#include "line-index.h"                          // LineIndex

#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue
#include "smbase/sm-macros.h"                    // DMEMB

#include <optional>                              // std::optional


// A host and file name, and optional line and byte indices.
class HostFile_OptLineByte {
private:     // data
  // Host and file name.
  //
  // The name can be empty, which is unusual, but in one case is
  // effectively interpreted as naming the current directory.
  HostAndResourceName m_harn;

  // Optional 0-based line index.
  std::optional<LineIndex> m_lineIndex;

  // Optional 0-based byte index.
  //
  // Invariant: `m_byteIndex.has_value()` implies
  // `m_lineIndex.has_value()`.
  std::optional<ByteIndex> m_byteIndex;

public:      // funcs
  // The default ctor is required to allow this type to be used as a
  // parameter type for a signal.
  HostFile_OptLineByte()
    : m_harn(),
      m_lineIndex(),
      m_byteIndex()
  {
    selfCheck();
  }

  ~HostFile_OptLineByte()
  {}

  HostFile_OptLineByte(HostFile_OptLineByte const &obj)
    : DMEMB(m_harn),
      DMEMB(m_lineIndex),
      DMEMB(m_byteIndex)
  {
    selfCheck();
  }

  HostFile_OptLineByte(
    HostAndResourceName const &harn,
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
  HostAndResourceName const &getHarn() const
    { return m_harn; }
  std::optional<LineIndex> const &getLineIndexOpt() const
    { return m_lineIndex; }
  std::optional<ByteIndex> const &getByteIndexOpt() const
    { return m_byteIndex; }

  // Tests for member presence.
  bool hasLineIndex() const { return m_lineIndex.has_value(); }
  bool hasByteIndex() const { return m_byteIndex.has_value(); }

  // Return: getHarn().resourceName()
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
