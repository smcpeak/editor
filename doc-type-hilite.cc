// doc-type-hilite.cc
// Code for `doc-type-hilite` module.

#include "doc-type-hilite.h"           // this module

#include "c_hilite.h"                  // C_Highlighter
#include "diff-hilite.h"               // DiffHighlighter
#include "doc-type.h"                  // DocumentType
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

    case DocumentType::DT_C:
      return std::make_unique<C_Highlighter>(core);

    case DocumentType::DT_MAKEFILE:
      return std::make_unique<Makefile_Highlighter>(core);

    case DocumentType::DT_HASH_COMMENT:
      return std::make_unique<HashComment_Highlighter>(core);

    case DocumentType::DT_OCAML:
      return std::make_unique<OCaml_Highlighter>(core);

    case DocumentType::DT_PYTHON:
      return std::make_unique<Python_Highlighter>(core);

    case DocumentType::DT_DIFF:
      return std::make_unique<DiffHighlighter>();
  }
}


// EOF
