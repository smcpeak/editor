// host-and-resource-name.h
// Class HostAndResourceName.

#ifndef EDITOR_HOST_AND_RESOURCE_NAME_H
#define EDITOR_HOST_AND_RESOURCE_NAME_H

// editor
#include "host-name.h"                 // HostName

// smbase
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-compare.h"         // StrongOrdering, DEFINE_RELOPS_FROM_COMPARE_TO
#include "smbase/str.h"                // string

// libc++
#include <iosfwd>                      // std::ostream


// Pair of a host name and a resource found on that host.
class HostAndResourceName {
protected:   // data
  // Host that has the resource.
  HostName m_hostName;

  // Name of the resource that supplies the data.  This could be a file
  // name, but there are other possibilities.  See DocumentName.
  //
  // It can also be empty, in which case the name as a whole is regarded
  // as being empty.  In that case, 'm_hostName.isLocal()' is true.
  string m_resourceName;

public:      // methods
  // Create an empty name.
  HostAndResourceName();
  ~HostAndResourceName();

  // Create with specified elements.
  HostAndResourceName(HostName const &hostName,
                      string const &resourceName);

  HostAndResourceName(HostAndResourceName const &obj) = default;
  HostAndResourceName& operator=(HostAndResourceName const &obj) = default;

  // Assert invariants.
  void selfCheck() const;

  operator gdv::GDValue() const;

  // Make an object carrying 'filename' and a local host designator.
  static HostAndResourceName localFile(string const &filename);

  // Compare lexicographically.
  StrongOrdering compareTo(HostAndResourceName const &obj) const;

  // Get the host that contains the resource.
  HostName hostName() const            { return m_hostName; }

  bool isLocal() const                 { return m_hostName.isLocal(); }

  // True if the name is empty.
  bool empty() const                   { return m_resourceName.empty(); }

  // Get the resource that provides the document content.
  string resourceName() const          { return m_resourceName; }

  // Return a string suitable for naming this document within an error
  // message.
  string toString() const;

  // Print 'toString()' to 'os'.
  std::ostream& print(std::ostream &os) const;
};


DEFINE_RELOPS_FROM_COMPARE_TO(HostAndResourceName)


inline std::ostream& operator<< (std::ostream &os, HostAndResourceName const &doc)
{
  return doc.print(os);
}


#endif // EDITOR_HOST_AND_RESOURCE_NAME_H
