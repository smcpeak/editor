// editor-strutil.h
// String utilities for use in a text editor.

// This is as distinct from smbase/strutil.h, which are intended to be
// even more general-purpose, although over time I could see migrating
// some of these into smbase.

#ifndef EDITOR_STRUTIL_H
#define EDITOR_STRUTIL_H

// smbase
#include "smbase/str.h"                // string


// If the code point at text[byteOffset] is a C identifier character,
// as determined by 'isCIdentifierCharacter' (codepoint.h), return the
// largest string including it that is entirely composed of such
// characters.  Otherwise return "".  Returns "" for an offset that is
// negative or beyond the end of the input string.
//
// TODO UTF-8: Returns "" if the offset is not the start of a UTF-8
// octet sequence.
string cIdentifierAt(string const &text, int byteOffset);


#endif // EDITOR_STRUTIL_H
