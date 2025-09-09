// doc-type-detect.cc
// Code for `doc-type-detect` module.

#include "doc-type-detect.h"           // this module

#include "doc-name.h"                  // DocumentName

#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-regex.h"           // smbase::Regex
#include "smbase/string-util.h"        // endsWith

#include <cstring>                     // std::strrchr
#include <string>                      // std::string

using namespace smbase;

using std::string;


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


static bool isCppHeaderName(string const &basename)
{
  static char const * const cppHeaderNames[] = {
    "algorithm",
    "any",
    "array",
    "atomic",
    "barrier",
    "bit",
    "bitset",
    "cassert",
    "ccomplex",
    "cctype",
    "cerrno",
    "cfenv",
    "cfloat",
    "charconv",
    "chrono",
    "cinttypes",
    "ciso646",
    "climits",
    "clocale",
    "cmath",
    "codecvt",
    "compare",
    "complex",
    "concepts",
    "condition_variable",
    "coroutine",
    "csetjmp",
    "csignal",
    "cstdalign",
    "cstdarg",
    "cstdbool",
    "cstddef",
    "cstdint",
    "cstdio",
    "cstdlib",
    "cstring",
    "ctgmath",
    "ctime",
    "cuchar",
    "cwchar",
    "cwctype",
    "deque",
    "exception",
    "execution",
    "expected",
    "filesystem",
    "format",
    "forward_list",
    "fstream",
    "functional",
    "future",
    "initializer_list",
    "iomanip",
    "ios",
    "iosfwd",
    "iostream",
    "istream",
    "iterator",
    "latch",
    "limits",
    "list",
    "locale",
    "map",
    "memory",
    "memory_resource",
    "mutex",
    "new",
    "numbers",
    "numeric",
    "optional",
    "ostream",
    "queue",
    "random",
    "ranges",
    "ratio",
    "regex",
    "scoped_allocator",
    "semaphore",
    "set",
    "shared_mutex",
    "source_location",
    "span",
    "spanstream",
    "sstream",
    "stack",
    "stacktrace",
    "stdexcept",
    "stdfloat",
    "stop_token",
    "streambuf",
    "string",
    "string_view",
    "syncstream",
    "system_error",
    "thread",
    "tuple",
    "typeindex",
    "typeinfo",
    "type_traits",
    "unordered_map",
    "unordered_set",
    "utility",
    "valarray",
    "variant",
    "vector",
    "version",
  };

  return stringAmong(basename, cppHeaderNames, TABLESIZE(cppHeaderNames));
}


DocumentType detectDocumentType(DocumentName const &docName)
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
      "ev",        // Also GDVN.
      "gr",
      "i",
      "ii",
      "h",
      "hh",
      "hpp",
      "gdvn",      // C/C++ is almost right (except for nested comments).
      "java",      // C/C++ highlighting is better than none.
      "json",      // Should work fine.
      "lex",
      "tcc",
      "y",
    };
    if (stringAmong(ext, cppExts, TABLESIZE(cppExts))) {
      return KDT_C;
    }

    if (streq(ext, "mk")) {
      return KDT_MAKEFILE;
    }

    static char const * const hashCommentExts[] = {
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

  string basename = SMFileUtil().splitPathBase(filename);
  if (isCppHeaderName(basename)) {
    return KDT_C;
  }

  return KDT_UNKNOWN;
}


// EOF
