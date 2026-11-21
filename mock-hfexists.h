// mock-hfexists.h
// `MockHFExists` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_MOCK_HFEXISTS_H
#define EDITOR_MOCK_HFEXISTS_H

// editor
#include "file-mod-time.h"             // FileTimestamp
#include "ihfexists.h"                 // IHFExists
#include "host-and-resource-name.h"    // HostAndResourceName

// smbase
#include "smbase/std-optional-fwd.h"   // std::optional [f]

// libc++
#include <map>                         // std::set


// Class that recognizes a fixed set of host+file pairs.
class MockHFExists : public IHFExists {
public:      // data
  // Map from recognized names to timestamps.
  std::map<HostAndResourceName, FileTimestamp> m_existingHARNs;

public:      // methods
  // Add `harn` to `m_existingHARNs`, using its `size()+1` as the
  // timestamp.  This is useful when, in a test, we just care about the
  // set of files, not their timestamps.
  void add(HostAndResourceName const &harn);

  // Add and specify the timestamp.  `harn` must not already be mapped.
  void addTS(HostAndResourceName const &harn, FileTimestamp ts);

  // Change an existing timestamp.
  void setTS(HostAndResourceName const &harn, FileTimestamp ts);

  // IHFExists methods
  virtual std::optional<FileTimestamp> hfModTime(
    HostAndResourceName const &harn) override;
};


#endif // EDITOR_MOCK_HFEXISTS_H
