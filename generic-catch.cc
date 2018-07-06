// generic-catch.cc
// code for generic-catch.h

#include "generic-catch.h"             // this module

#include "sm-iostream.h"               // cerr

#include <QMessageBox>


void printUnhandled(QWidget *parent, char const *msg)
{
  cerr << "Unhandled exception: " << msg << endl;

  static int count = 0;
  if (++count >= 5) {
    // Stop showing dialog boxes at a certain point.
    return;
  }

  QMessageBox::information(parent, "Oops",
    QString(stringc << "Unhandled exception: " << msg << "\n"
                    << "Save your files if you can!"));
}


// EOF
