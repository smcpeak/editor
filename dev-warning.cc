// dev-warning.cc
// code for dev-warning.h

#include "dev-warning.h"               // this module

#include "sm-iostream.h"               // cerr


void devWarning(char const *file, int line, char const *msg)
{
  // For now we just print to stderr.
  cerr << "DEV_WARNING: " << file << ':' << line << ": " << msg << endl;
}


// EOF
