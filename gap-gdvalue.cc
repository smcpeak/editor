// gap-gdvalue.cc
// Code for `gap-gdvalue` module.

#include "gap-gdvalue.h"               // this module

#include "gap.h"                       // GapArray

#include "smbase/gdvalue.h"            // gdv::GDValue

using namespace gdv;


template <>
gdv::GDValue toGDValue(GapArray<char> const &arr)
{
  GDVString s;

  for (int i=0; i < arr.length(); ++i) {
    s.push_back(arr.get(i));
  }

  return GDValue(s);
}


// EOF
