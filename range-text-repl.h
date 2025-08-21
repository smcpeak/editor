// range-text-repl.h
// `RangeTextReplacement` class for describing a text replacement.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RANGE_TEXT_REPL_H
#define EDITOR_RANGE_TEXT_REPL_H

#include "range-text-repl-fwd.h"       // fwds for this module

#include "textmcoord.h"                // TextMCoordRange

#include <optional>                    // std::optional
#include <string>                      // std::string


// A range and its replacement text.
//
// This is conceptually analogous to the `TextDocumentChange` hierarchy,
// except everything is captured in one object for use with
// `TextDocument` rather than `TextDocumentCore`.
//
// This is also how LSP expresses changes, so is useful for that too.
//
class RangeTextReplacement {
public:      // data
  // The range to replace, or absent to replace everything.
  std::optional<TextMCoordRange> m_range;

  // The new text.
  std::string m_text;

public:      // methods
  ~RangeTextReplacement();

  explicit RangeTextReplacement(
    std::optional<TextMCoordRange> range,
    std::string const &text);

  explicit RangeTextReplacement(
    std::optional<TextMCoordRange> range,
    std::string &&text);

  RangeTextReplacement(RangeTextReplacement &&obj);
  RangeTextReplacement &operator=(RangeTextReplacement &&obj);
};


#endif // EDITOR_RANGE_TEXT_REPL_H
