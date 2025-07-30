// debug-values.h
// DEBUG_VALUES family of macros.

// This is a candidate to move to smbase.

#ifndef DEBUG_VALUES_H
#define DEBUG_VALUES_H


// When used in a std::ostringstream or ostream context, inserts the
// expression as text and then a value.  For simplicity, the value is
// surrounded by quotes, even non-strings, but not escaped.
//
// The intent is to use this in a TRACE or DEV_WARNING call to print
// values for use in debugging.  End users are not expected to see
// these strings (except when the program is self-reporting a bug).
#define DEBUG_VALUES(expr) #expr "=\"" << (expr) << '"'

#define DEBUG_VALUES2(e1, e2) \
  #e1 "=\"" << (e1) << "\" " \
  #e2 "=\"" << (e2) << '"'

#define DEBUG_VALUES3(e1, e2, e3) \
  #e1 "=\"" << (e1) << "\" " \
  #e2 "=\"" << (e2) << "\" " \
  #e3 "=\"" << (e3) << '"'

#define DEBUG_VALUES4(e1, e2, e3, e4) \
  #e1 "=\"" << (e1) << "\" " \
  #e2 "=\"" << (e2) << "\" " \
  #e3 "=\"" << (e3) << "\" " \
  #e4 "=\"" << (e4) << '"'


#endif // DEBUG_VALUES_H
