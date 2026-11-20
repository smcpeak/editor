// mock-hfexists.h
// `MockHFExists` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_MOCK_HFEXISTS_H
#define EDITOR_MOCK_HFEXISTS_H

// editor
#include "ihfexists.h"                 // IHFExists
#include "host-and-resource-name.h"    // HostAndResourceName

// libc++
#include <set>                         // std::set


// Class that recognizes a fixed set of host+file pairs.
class MockHFExists : public IHFExists {
public:      // data
  // Set of recognized names.
  std::set<HostAndResourceName> m_existingHARNs;

public:      // methods
  virtual bool hfExists(HostAndResourceName const &harn) override;
};


#endif // EDITOR_MOCK_HFEXISTS_H
