// ui-shell-queryable.cc
// Code for `ui-shell-queryable` module.

#include "ui-shell-queryable.h"        // this module

// editor
#include "ui-shell-result.h"           // UIShellResult

// smbase
#include "smbase/exc.h"                // smbase::xmessage
#include "smbase/gdvalue.h"            // gdv::GDValue, etc.
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/stringb.h"            // stringb

// libc++
#include <exception>                   // std::exception


using namespace gdv;
using namespace smbase;


GDValue UIShellQueryable::uiShellQuery(std::string const &state)
{
  xmessage(stringb("unknown state: " << doubleQuote(state)));
  return GDValue();   // not reached
}


UIShellResult UIShellQueryable::tryUIShellQuery(std::string const &state)
{
  try {
    return UIShellResult::makeLeft(uiShellQuery(state));
  }
  catch (std::exception &e) {
    return UIShellResult::makeRight(e.what());
  }
  catch (...) {
    return UIShellResult::makeRight("caught unknown exception!");
  }
}


bool UIShellQueryable::wantResizeEventsRecorded()
{
  return false;
}


// EOF
