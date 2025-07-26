// doc-type-detect.cc
// Code for `doc-type-detect` module.

#include "doc-type-detect.h"           // this module

#include "doc-name.h"                  // DocumentName

#include "smbase/sm-regex.h"           // smbase::Regex
#include "smbase/string-util.h"        // endsWith

#include <string>                      // std::string

using namespace smbase;


bool isDiffName(DocumentName const &docName)
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


// EOF
