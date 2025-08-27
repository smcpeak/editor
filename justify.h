// justify.h
// Text justification routines.

#ifndef EDITOR_JUSTIFY_H
#define EDITOR_JUSTIFY_H

#include "line-index-fwd.h"            // LineIndex [n]
#include "td-editor-fwd.h"             // TextDocumentEditor [n]

#include "smbase/std-string-fwd.h"     // std::string [n]
#include "smbase/std-vector-fwd.h"     // stdfwd::vector [n]


// Given 'originalContent', rearrange its whitespace to obtain a
// set of lines that have 'desiredWidth' or less.  Place the
// result in 'justifiedContent'.
//
// If two adjacent words start on the same line, and are not
// split across lines, the number of spaces between them shall
// be preserved.  This provision is based on the idea that the
// author originally put the number of spaces they want to use,
// whether or not that matches my preferred convention.
//
// However, when we have to synthesize space, we insert two
// spaces for what appear to be sentence boundaries, reflecting
// my own typographical preferences.
void justifyTextLines(
  stdfwd::vector<std::string> &justifiedContent,
  stdfwd::vector<std::string> const &originalContent,
  int desiredWidth);


// Heuristically identify the textual framing used for the text at and
// surrounding 'originLine', separate it from the content, wrap the
// content to the result will fit (framing plus wrapped text) into
// 'desiredWidth' columns if possible, and replace the identified region
// with its wrapped version.  Return false if we could not identify a
// wrappable region.
//
// Within the framing, tab characters are treated as representing 8
// columns of width.  Elsewhere they are treated as just 1 (which is
// arguably a bug).
bool justifyNearLine(TextDocumentEditor &tde, LineIndex originLine,
                     int desiredWidth);


#endif // EDITOR_JUSTIFY_H
