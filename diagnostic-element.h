// diagnostic-element.h
// `DiagnosticElement`, one element of a compiler diagnostic message.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DIAGNOSTIC_ELEMENT_H
#define EDITOR_DIAGNOSTIC_ELEMENT_H

#include "diagnostic-element-fwd.h"    // fwds for this module

#include <QString>


// One element of a diagnostic message.
//
// TODO: Make this a global class called `DiagnosticElement` that
// carries `std::string`, and combines dir+file.
struct DiagnosticElement {
  // Absolute directory path containing `file`.  It ends with a slash.
  //
  // TODO: Combine with `m_file`, change to `std::string`.
  //
  // TODO: Use `DocumentName`.
  QString m_dir;

  // Name of a file within `dir`
  QString m_file;

  // 1-based line number within `file` where the syntax of interest
  // is.
  int m_line;

  // The relevance of the indicated line.  This might be very long,
  // often hundreds and occasionally more than 1000 characters, due to
  // the "explosive" nature of C++ template error messages.
  QString m_message;
};


#endif // EDITOR_DIAGNOSTIC_ELEMENT_H
