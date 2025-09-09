// doc-type-hilite.cc
// Code for `doc-type-hilite` module.

#include "doc-type-hilite.h"           // this module

#include "c_hilite.h"                  // C_Highlighter
#include "diff-hilite.h"               // DiffHighlighter
#include "hashcomment_hilite.h"        // HashComment_Highlighter
#include "makefile_hilite.h"           // Makefile_Highlighter
#include "ocaml_hilite.h"              // OCaml_Highlighter
#include "python_hilite.h"             // Python_Highlighter

#include <memory>                      // std::unique_ptr


std::unique_ptr<Highlighter> makeHighlighterForLanguage(
  DocumentType kdt,
  TextDocumentCore const &core)
{
  switch (kdt) {
    default:
      return nullptr;

    case KDT_C:
      return std::make_unique<C_Highlighter>(core);

    case KDT_MAKEFILE:
      return std::make_unique<Makefile_Highlighter>(core);

    case KDT_HASH_COMMENT:
      return std::make_unique<HashComment_Highlighter>(core);

    case KDT_OCAML:
      return std::make_unique<OCaml_Highlighter>(core);

    case KDT_PYTHON:
      return std::make_unique<Python_Highlighter>(core);

    case KDT_DIFF:
      return std::make_unique<DiffHighlighter>();
  }
}


// EOF
