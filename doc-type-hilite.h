// doc-type-hilite.h
// Make a highlighter for a document type.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_DOC_TYPE_HILITE_H
#define EDITOR_DOC_TYPE_HILITE_H

#include "doc-type-fwd.h"              // DocumentType [n]
#include "hilite-fwd.h"                // Highlighter [n]
#include "td-core-fwd.h"               // TextDocumentCore [n]

#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr [n]


// Get the highlighter for `kdt`.  It will be attached to `core` for its
// highlighting work (if the highlighter needs such access).  May return
// nullptr (as is the case for `DT_UNKNOWN`, but possibly others).
stdfwd::unique_ptr<Highlighter> makeHighlighterForLanguage(
  DocumentType kdt,
  TextDocumentCore const &core);


#endif // EDITOR_DOC_TYPE_HILITE_H
