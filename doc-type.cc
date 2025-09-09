// doc-type.cc
// Code for `doc-type` module.

#include "doc-type.h"                  // this module

#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR

#include <iostream>                    // std::ostream


DEFINE_ENUMERATION_TO_STRING_OR(
  DocumentType,
  DocumentType::NUM_KNOWN_DOCUMENT_TYPES,
  (
    "KDT_UNKNOWN",
    "KDT_C",
    "KDT_MAKEFILE",
    "KDT_HASH_COMMENT",
    "KDT_OCAML",
    "KDT_PYTHON",
    "KDT_DIFF",
  ),
  "KDT_invalid"
)


std::ostream &operator<<(std::ostream &os, DocumentType dt)
{
  return os << toString(dt);
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
