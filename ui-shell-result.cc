// ui-shell-result.cc
// Code for `ui-shell-result` module.

#include "ui-shell-result.h"           // this module

// smbase
#include "smbase/gdvalue-either.h"     // gdv::toGDValue(smbase::Either)


gdv::GDValue toGDValue(UIShellResult const &res)
{
  return gdv::toGDValue(res);
}


// EOF
