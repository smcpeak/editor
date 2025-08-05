// hilite.cc
// Code for `hilite` module.

#include "hilite.h"                    // this module

#include "smbase/gdvalue.h"            // gdv::GDValue

using namespace gdv;


Highlighter::~Highlighter()
{}


Highlighter::operator gdv::GDValue() const
{
  return GDValue(GDVSymbol(highlighterName()));
}


// EOF
