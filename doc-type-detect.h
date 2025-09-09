// doc-type-detect.h
// Document type detection.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_DETECT_H
#define EDITOR_DOC_TYPE_DETECT_H

#include "doc-name-fwd.h"              // DocumentName


// All the kinds of documents I know about.
enum KnownDocumentType {
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
char const *toString(KnownDocumentType kdt);

// Return a string like "None" or "C++".
char const *languageName(KnownDocumentType kdt);


// Iterate with `kdt` over all `KnownDocumentType`s.
#define FOR_EACH_KNOWN_DOCUMENT_TYPE(kdt)   \
  for (KnownDocumentType kdt = KDT_UNKNOWN; \
       kdt < NUM_KNOWN_DOCUMENT_TYPES;      \
       kdt = (KnownDocumentType)(kdt + 1))


// Determine the document type based on its name or command line.
// Return `KDT_UNKNOWN` if it cannot be determined.
KnownDocumentType detectDocumentType(DocumentName const &docName);


#endif // EDITOR_DOC_TYPE_DETECT_H
