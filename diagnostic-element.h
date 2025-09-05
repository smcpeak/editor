// diagnostic-element.h
// `DiagnosticElement`, one element of a compiler diagnostic message.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DIAGNOSTIC_ELEMENT_H
#define EDITOR_DIAGNOSTIC_ELEMENT_H

#include "diagnostic-element-fwd.h"    // fwds for this module

#include "host-and-resource-name.h"    // HostAndResourceName
#include "line-number.h"               // LineNumber

#include <string>                      // std::string


// One element of a diagnostic message.
struct DiagnosticElement {
  // Host and file the message refers to.
  HostAndResourceName m_harn;

  // 1-based line number within `m_harn` where the syntax of interest
  // is.
  //
  // TODO: Change to `LineIndex`.
  LineNumber m_line;

  // The relevance of the indicated line.  This might be very long,
  // often hundreds and occasionally more than 1000 characters, due to
  // the "explosive" nature of C++ template error messages.
  std::string m_message;
};


#endif // EDITOR_DIAGNOSTIC_ELEMENT_H
