// textmcoord-map-test.cc
// Tests for `textmcoord-map` module.

#include "textmcoord-map.h"            // module under test

#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // ARGS_MAIN

#include <iostream>                    // std::cout


using namespace gdv;


OPEN_ANONYMOUS_NAMESPACE


void test1()
{
  TextMCoordMap m;
  toGDValue(m).writeLines(std::cout);
}


void entry(int argc, char **argv)
{
  test1();
}


CLOSE_ANONYMOUS_NAMESPACE


ARGS_TEST_MAIN


// EOF
