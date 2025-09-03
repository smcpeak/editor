// editor-strutil.cc
// code for editor-strutil.h

#include "editor-strutil.h"            // this module

#include "byte-count.h"                // ByteCount
#include "byte-difference.h"           // ByteDifference
#include "byte-index.h"                // ByteIndex

// smbase
#include "smbase/codepoint.h"          // isCIdentifierCharacter

// libc++
#include <string>                      // std::string


std::string cIdentifierAt(std::string const &text, ByteIndex byteOffset)
{
  ByteCount len = sizeBC(text);
  if (byteOffset < len) {
    if (!isCIdentifierCharacter(at(text, byteOffset))) {
      return "";
    }

    ByteIndex start = byteOffset;
    while (start > 0 && isCIdentifierCharacter(at(text, start.pred()))) {
      start--;
    }

    // One past the last character to return.
    ByteIndex end = byteOffset.succ();
    while (end < len && isCIdentifierCharacter(at(text, end))) {
      end++;
    }

    return substr(text, start, ByteCount(end-start));
  }
  else {
    return "";
  }
}


// EOF
