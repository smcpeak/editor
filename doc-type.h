// doc-type.h
// `DocumentType` enumeration of known document types and languages.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_H
#define EDITOR_DOC_TYPE_H


// All the kinds of documents I know about.
enum DocumentType {
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
char const *toString(DocumentType kdt);

// Return a string like "None" or "C++".
char const *languageName(DocumentType kdt);


// Iterate with `kdt` over all `DocumentType`s.
#define FOR_EACH_KNOWN_DOCUMENT_TYPE(kdt)   \
  for (DocumentType kdt = KDT_UNKNOWN; \
       kdt < NUM_KNOWN_DOCUMENT_TYPES;      \
       kdt = (DocumentType)(kdt + 1))


#endif // EDITOR_DOC_TYPE_H
