// ihfexists.h
// `IHFExists` interface.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_IHFEXISTS_H
#define EDITOR_IHFEXISTS_H

#include "ihfexists-fwd.h"                       // fwds for this module

// editor
#include "host-and-resource-name-fwd.h"          // HostAndResourceName [f]


// Interface with which to test the existence of a host+file.
class IHFExists {
public:      // methods
  virtual ~IHFExists();

  /* From the client's perspective, invoking this method does not change
     the state of the receiver object.  However, this method is not
     `const` because the main implementor of it, `RFA_SyncMultiClient`, needs
     to invoke non-const helper methods due to maintaining state related
     to the communication used to answer the question.  I'm not sure if
     there is or should be a better way to handle this situation.
  */
  virtual bool hfExists(HostAndResourceName const &harn) = 0;
};


#endif // EDITOR_IHFEXISTS_H
