// generic-catch.h
// Module to help catch exceptions before they reach Qt code.

#ifndef GENERIC_CATCH_H
#define GENERIC_CATCH_H

#include "exc.h"                       // xBase

#include <QWidget>


// Display an unhandled exception error.
void printUnhandled(QWidget *parent, char const *msg);


// This goes at the top of any function called by Qt.
#define GENERIC_CATCH_BEGIN         \
  try {

// And this goes at the bottom.
#define GENERIC_CATCH_END           \
  }                                 \
  catch (xBase &x) {                \
    printUnhandled(this, toCStr(x.why()));  \
  }


#endif // GENERIC_CATCH_H
