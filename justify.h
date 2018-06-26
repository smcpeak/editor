// justify.h
// Text justification routines.

#ifndef JUSTIFY_H
#define JUSTIFY_H

#include "text-document-editor.h"      // TextDocumentEditor

#include "array.h"                     // ArrayStack
#include "str.h"                       // string


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
  ArrayStack<string> &justifiedContent,
  ArrayStack<string> const &originalContent,
  int desiredWidth);


// Heuristically identify the textual framing used for the text at
// and surrounding 'originLine', separate it from the content, wrap the
// content to the result will fit into 'desiredWidth' columns if
// possible, and replace the identified region with its wrapped
// version.  Return false if we could not identify a wrappable
// region.
bool justifyNearLine(TextDocumentEditor &tde, int originLine,
                     int desiredWidth);


#endif // JUSTIFY_H
