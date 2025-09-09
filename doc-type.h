// doc-type.h
// `DocumentType` enumeration of known document types and languages.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_H
#define EDITOR_DOC_TYPE_H

#include "doc-type-fwd.h"              // fwds for this module

#include <iosfwd>                      // std::ostream


// All the kinds of documents I know about.
enum class DocumentType : int {
  KDT_UNKNOWN,               // Unrecognized.

  KDT_C,                     // C or C++.
  KDT_MAKEFILE,
  KDT_HASH_COMMENT,          // Something that uses '#' for comments.
  KDT_OCAML,
  KDT_PYTHON,
  KDT_DIFF,                  // Unified `diff` output.

  NUM_KNOWN_DOCUMENT_TYPES
};

// Return a string like "KDT_C".
char const *toString(DocumentType dt);

// Write like `toString` does.
std::ostream &operator<<(std::ostream &os, DocumentType dt);

// Return a string like "None" or "C++".
char const *languageName(DocumentType dt);


// Iterate with `dt` over all `DocumentType`s.
#define FOR_EACH_KNOWN_DOCUMENT_TYPE(dt)            \
  for (DocumentType dt = DocumentType::KDT_UNKNOWN; \
       dt < DocumentType::NUM_KNOWN_DOCUMENT_TYPES; \
       dt = DocumentType(int(dt) + 1))


#endif // EDITOR_DOC_TYPE_H
