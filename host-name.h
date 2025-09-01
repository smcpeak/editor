// host-name.h
// HostName class.

#ifndef EDITOR_HOST_NAME_H
#define EDITOR_HOST_NAME_H

#include "host-name-fwd.h"             // fwds for this module

// smbase
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-compare.h"         // StrongOrdering, DEFINE_RELOPS_FROM_COMPARE_TO
#include "smbase/str.h"                // string

// libc++
#include <iosfwd>                      // std::ostream


// Name of a machine or server that can respond to VFS queries.
//
// I expect to at some point expand this with protocol information,
// although currently it only knows about SSH.
class HostName {
private:     // data
  // Host meant to be accessed via SSH, or the empty string to signify
  // the local machine.
  string m_sshHostName;

protected:   // methods
  // Protected constructor to force the use of one of the static
  // constructor methods.
  HostName();

public:      // methods
  ~HostName();

  HostName(HostName const &obj) = default;
  HostName& operator=(HostName const &obj) = default;

  operator gdv::GDValue() const;

  // Name the local host.
  static HostName asLocal();

  // Name a resource to access via SSH.
  static HostName asSSH(string const &hostname);

  StrongOrdering compareTo(HostName const &obj) const;

  // True if this names the local host.
  bool isLocal() const { return m_sshHostName.empty(); }

  // True if this names a resource accessible via SSH.
  bool isSSH() const { return !isLocal(); }

  // Get the host name for use with 'ssh'.
  //
  // Requires: isSSH().
  string getSSHHostName() const;

  // Return "local" or "ssh:<hostname>" (without quotes).
  string toString() const;

  // Print 'toString()' to 'os'.
  std::ostream& print(std::ostream &os) const;
};


DEFINE_RELOPS_FROM_COMPARE_TO(HostName)


inline std::ostream& operator<< (std::ostream &os, HostName const &doc)
{
  return doc.print(os);
}


#endif // EDITOR_HOST_NAME_H
