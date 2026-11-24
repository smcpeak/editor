// ui-shell-queryable.cc
// Code for `ui-shell-queryable` module.

#include "ui-shell-queryable.h"        // this module

// smbase
#include "smbase/gdvalue.h"                      // gdv::GDValue, etc.
#include "smbase/string-util.h"                  // doubleQuote
#include "smbase/stringb.h"                      // stringb

// Qt
#include <QImage>

// libc++
#include <exception>                             // std::exception


using namespace gdv;


/*static*/ GDValue UIShellQueryable::errorTuple(std::string const &msg)
{
  return GDVTaggedTuple("Error"_sym, GDVTuple({toGDValue(msg)}));
}


GDValue UIShellQueryable::uiShellQuery(std::string const &state)
{
  return errorTuple(stringb("unknown state: " << doubleQuote(state)));
}


GDValue UIShellQueryable::tryUIShellQuery(std::string const &state)
{
  try {
    return uiShellQuery(state);
  }
  catch (std::exception &e) {
    return errorTuple(e.what());
  }
}


QImage UIShellQueryable::uiShellImage(std::string const &)
{
  return QImage();
}

bool UIShellQueryable::wantResizeEventsRecorded()
{
  return false;
}


// EOF
