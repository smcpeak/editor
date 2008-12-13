// qtutil.h
// some miscellaneous utilities for Qt

#ifndef QTUTIL_H
#define QTUTIL_H

#include <qnamespace.h>       // ButtonState, Key
#include <qstring.h>          // QString
#include "str.h"              // string

class QKeyEvent;              // qevent.h


string toString(Qt::ButtonState s);

char const *toString(Qt::Key k);

string toString(QKeyEvent const &k);

QString toQString(string const &s);


#endif // QTUTIL_H
