// doc-type.cc
// Code for `doc-type` module.

#include "doc-type.h"                  // this module

#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR


DEFINE_ENUMERATION_TO_STRING_OR(
  DocumentType,
  NUM_KNOWN_DOCUMENT_TYPES,
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


char const *languageName(DocumentType sl)
{
  RETURN_ENUMERATION_STRING_OR(
    DocumentType,
    NUM_KNOWN_DOCUMENT_TYPES,
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
