// ui-shell-gui-queryable.cc
// Code for `ui-shell-gui-queryable` module.

#include "ui-shell-gui-queryable.h"    // this module

// smbase
#include "smbase/exc.h"                // smbase::xmessage
#include "smbase/string-util.h"        // doubleQuote
#include "smbase/stringb.h"            // stringb

// Qt
#include <QImage>

// libc++
#include <string>                      // std::string


using namespace smbase;


QImage UIShellGUIQueryable::uiShellImage(std::string const &what)
{
  xmessage(stringb("unknown image: " << doubleQuote(what)));
  return QImage();    // not reached
}


// EOF
