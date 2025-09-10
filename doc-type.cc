// doc-type.cc
// Code for `doc-type` module.

#include "doc-type.h"                  // this module

#include "smbase/gdvalue.h"            // gdv::GDValue [n]
#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR

#include <iostream>                    // std::ostream

using namespace gdv;


DEFINE_ENUMERATION_TO_STRING_OR(
  DocumentType,
  DocumentType::NUM_KNOWN_DOCUMENT_TYPES,
  (
    "DT_UNKNOWN",
    "DT_C",
    "DT_MAKEFILE",
    "DT_HASH_COMMENT",
    "DT_OCAML",
    "DT_PYTHON",
    "DT_DIFF",
  ),
  "DT_invalid"
)


std::ostream &operator<<(std::ostream &os, DocumentType dt)
{
  return os << toString(dt);
}


gdv::GDValue toGDValue(DocumentType dt)
{
  return GDVSymbol(toString(dt));
}


char const *languageName(DocumentType sl)
{
  RETURN_ENUMERATION_STRING_OR(
    DocumentType,
    DocumentType::NUM_KNOWN_DOCUMENT_TYPES,
    (
      "None",
      "C++",
      "Makefile",
      "Hash comment",
      "OCaml",
      "Python",
      "Unified diff",
    ),
    sl,
    "<invalid language>"
  )
}


// EOF
