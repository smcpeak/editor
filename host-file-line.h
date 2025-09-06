// host-file-line.h
// `HostFileLine`, a host/file/line triple.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_HOST_FILE_LINE_H
#define EDITOR_HOST_FILE_LINE_H

#include "host-file-line-fwd.h"        // fwds for this module

#include "host-and-resource-name.h"    // HostAndResourceName
#include "line-index.h"                // LineIndex

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]


// Host, file, line index.
class HostFileLine {
public:      // data
  // Host and file.
  HostAndResourceName m_harn;

  // 0-based line index.
  LineIndex m_lineIndex;

public:      // methods
  // ---- create-tuple-class: declarations for HostFileLine +compare +write +gdvWrite
  /*AUTO_CTC*/ explicit HostFileLine(HostAndResourceName const &harn, LineIndex const &lineIndex);
  /*AUTO_CTC*/ HostFileLine(HostFileLine const &obj) noexcept;
  /*AUTO_CTC*/ HostFileLine &operator=(HostFileLine const &obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(HostFileLine const &a, HostFileLine const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(HostFileLine)
  /*AUTO_CTC*/ // For +write:
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, HostFileLine const &obj);
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;

  HostAndResourceName const &getHarn() const
    { return m_harn; }
  LineIndex const &getLineIndex() const
    { return m_lineIndex; }
};


#endif // EDITOR_HOST_FILE_LINE_H
