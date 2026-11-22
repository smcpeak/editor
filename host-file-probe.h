// host-file-probe.h
// `HostFileProbe` interface.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_HOST_FILE_PROBE_H
#define EDITOR_HOST_FILE_PROBE_H

#include "host-file-probe-fwd.h"                 // fwds for this module

// editor
#include "file-mod-time.h"                       // FileTimestamp
#include "host-and-resource-name-fwd.h"          // HostAndResourceName [f]

// smbase
#include "smbase/std-optional-fwd.h"             // std::optional [f]


/*
  Interface that can be used to do certain file system checks, where the
  file can be on any host.  This is used to provide a low-dependency
  distributed file access mechanism to modules that don't want to know
  about the full RFA system.

  The methods in this class are not marked `const` because, although
  from the client's perspective they are not intended to cause changes,
  the implementor may maintain various communication state variables.

  Furthermore, the methods of this class don't return error indications
  because they are used in contexts where we don't want to report them
  anyway.  Error conditions are folded into the return values in a way
  documented with each method.  Consequently, this interface is meant
  for "discretionary" checks, where errors can be safely folded into
  non-error cases.
*/
class HostFileProbe {
public:      // methods
  virtual ~HostFileProbe();

  /*
    True if `harn` can be confirmed to exist as a regular file.  A false
    return covers various error cases and user cancelation.

    This is implemented in terms of `hfModTime`.
  */
  bool hfExists(HostAndResourceName const &harn);

  /*
    If we can locate `harn` as a regular file and get its modification
    time, that is returned as Unix time.  Otherwise (file missing, wrong
    kind, comms error, etc.) return nullopt.
  */
  virtual std::optional<FileTimestamp> hfModTime(
    HostAndResourceName const &harn) = 0;
};


#endif // EDITOR_HOST_FILE_PROBE_H
