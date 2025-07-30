// doc-type-detect.cc
// Code for `doc-type-detect` module.

#include "doc-type-detect.h"           // this module

#include "doc-name.h"                  // DocumentName

#include "smbase/sm-macros.h"          // DEFINE_ENUMERATION_TO_STRING_OR
#include "smbase/sm-regex.h"           // smbase::Regex
#include "smbase/string-util.h"        // endsWith

#include <cstring>                     // std::strrchr
#include <string>                      // std::string

using namespace smbase;

using std::string;


DEFINE_ENUMERATION_TO_STRING_OR(
  KnownDocumentType,
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


static bool isDiffName(DocumentName const &docName)
{
  std::string name = docName.resourceName();

  if (docName.hasFilename()) {
    // Recognize common diff file extensions.
    return endsWith(name, ".diff") ||
           endsWith(name, ".patch");
  }

  else {
    // For a process, look for "diff" surrounded by word boundaries.
    Regex r(R"(\bdiff\b)");
    return r.searchB(name);
  }
}


static bool stringAmong(string const &str, char const * const *table,
                        int tableSize)
{
  for (int i=0; i < tableSize; i++) {
    if (str == table[i]) {
      return true;
    }
  }
  return false;
}


KnownDocumentType detectDocumentType(DocumentName const &docName)
{
  // This handles both "foo.diff" and "git diff [<fname>]".
  if (isDiffName(docName)) {
    return KDT_DIFF;
  }

  if (!docName.hasFilename()) {
    return KDT_UNKNOWN;
  }

  // Get file extension.
  string filename = docName.filename();
  char const *dot = std::strrchr(filename.c_str(), '.');
  if (dot) {
    string ext = string(dot+1);

    static char const * const cppExts[] = {
      "ast",
      "c",
      "cc",
      "cpp",
      "gr",
      "i",
      "ii",
      "h",
      "hh",
      "hpp",
      "java",      // C/C++ highlighting is better than none.
      "lex",
      "y",
    };
    if (stringAmong(ext, cppExts, TABLESIZE(cppExts))) {
      return KDT_C;
    }

    if (streq(ext, "mk")) {
      return KDT_MAKEFILE;
    }

    static char const * const hashCommentExts[] = {
      "ev",
      "pl",
      "sh",
    };
    if (stringAmong(ext, hashCommentExts, TABLESIZE(hashCommentExts))) {
      return KDT_HASH_COMMENT;
    }

    static char const * const ocamlExts[] = {
      "ml",
      "mli",
    };
    if (stringAmong(ext, ocamlExts, TABLESIZE(ocamlExts))) {
      return KDT_OCAML;
    }

    static char const * const pythonExts[] = {
      "py",
      "pyi",
    };
    if (stringAmong(ext, pythonExts, TABLESIZE(pythonExts))) {
      return KDT_PYTHON;
    }
  }

  if (endsWith(filename, "Makefile")) {
    return KDT_MAKEFILE;
  }


  return KDT_UNKNOWN;
}


// EOF
