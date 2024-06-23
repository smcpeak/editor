// editor-strutil.cc
// code for editor-strutil.h

#include "editor-strutil.h"            // this module

// editor
#include "codepoint.h"                 // isCIdentifierCharacter

// libc++
#include <string>                      // std::string


string cIdentifierAt(string const &text, int byteOffset)
{
  int len = text.length();
  if (0 <= byteOffset && byteOffset < len) {
    if (!isCIdentifierCharacter(text[byteOffset])) {
      return "";
    }

    int start = byteOffset;
    while (start > 0 && isCIdentifierCharacter(text[start-1])) {
      start--;
    }

    // One past the last character to return.
    int end = byteOffset+1;
    while (end < len && isCIdentifierCharacter(text[end])) {
      end++;
    }

    return text.substr(start, end-start);
  }
  else {
    return "";
  }
}


// EOF
