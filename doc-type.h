// doc-type.h
// `DocumentType` enumeration of known document types and languages.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_H
#define EDITOR_DOC_TYPE_H

#include "doc-type-fwd.h"              // fwds for this module

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]

#include <iosfwd>                      // std::ostream [n]


// All the kinds of documents I know about.
enum class DocumentType : int {
  DT_PLAIN_TEXT,             // Default.

  DT_C_LIKE,                 // C-like for highlighting only.
  DT_CPP,                    // C++; can use `clangd` language services.
  DT_MAKEFILE,
  DT_HASH_COMMENT,           // Something that uses '#' for comments.
  DT_OCAML,
  DT_PYTHON,                 // Can use `pylsp` language services.
  DT_DIFF,                   // Unified `diff` output.

  NUM_KNOWN_DOCUMENT_TYPES
};

// Return a string like "DT_CPP".
char const *toString(DocumentType dt);

// Write like `toString` does.
std::ostream &operator<<(std::ostream &os, DocumentType dt);

// Return a symbol with the `toString` name.
gdv::GDValue toGDValue(DocumentType dt);

// Return a string like "Plain text" or "C++".
char const *languageName(DocumentType dt);


// Iterate with `dt` over all `DocumentType`s.
#define FOR_EACH_KNOWN_DOCUMENT_TYPE(dt)              \
  for (DocumentType dt = DocumentType::DT_PLAIN_TEXT; \
       dt < DocumentType::NUM_KNOWN_DOCUMENT_TYPES;   \
       dt = DocumentType(int(dt) + 1))


#endif // EDITOR_DOC_TYPE_H
