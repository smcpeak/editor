// dev-warning.h
// "Developer warning" mechanism.

// I am putting this into the editor for now, but this could move to
// smbase in the future if it works out.

#ifndef DEV_WARNING_H
#define DEV_WARNING_H

#include "str.h"                       // stringbc


// Print or log a warning originating at file/line.  This is something
// that the developer thinks should not or cannot happen, but is
// recoverable (no need to abort or throw), and the end user would not
// know or care about it.
void devWarning(char const *file, int line, char const *msg);


// Macro for convenient usage.
#define DEV_WARNING(msg) \
  devWarning(__FILE__, __LINE__, stringbc(msg)) /* user ; */



#endif // DEV_WARNING_H
