// range-text-repl.h
// `RangeTextReplacement` class for describing a text replacement.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RANGE_TEXT_REPL_H
#define EDITOR_RANGE_TEXT_REPL_H

#include "range-text-repl-fwd.h"       // fwds for this module

#include "textmcoord.h"                // TextMCoordRange

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser [n]

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
class RangeTextReplacement final {
public:      // data
  // The range to replace, or absent to replace everything.
  std::optional<TextMCoordRange> m_range;

  // The new text.
  std::string m_text;

public:      // methods
  // create-tuple-class: declarations for RangeTextReplacement +move +gdvWrite +gdvRead
  /*AUTO_CTC*/ explicit RangeTextReplacement(std::optional<TextMCoordRange> const &range, std::string const &text);
  /*AUTO_CTC*/ explicit RangeTextReplacement(std::optional<TextMCoordRange> &&range, std::string &&text);
  /*AUTO_CTC*/ RangeTextReplacement(RangeTextReplacement const &obj) noexcept;
  /*AUTO_CTC*/ RangeTextReplacement(RangeTextReplacement &&obj) noexcept;
  /*AUTO_CTC*/ RangeTextReplacement &operator=(RangeTextReplacement const &obj) noexcept;
  /*AUTO_CTC*/ RangeTextReplacement &operator=(RangeTextReplacement &&obj) noexcept;
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, RangeTextReplacement const &obj);
  /*AUTO_CTC*/ // For +gdvRead:
  /*AUTO_CTC*/ explicit RangeTextReplacement(gdv::GDValueParser const &p);
};


// Needed for use as data in `editor-command.ast`.
std::string toString(RangeTextReplacement const &obj);


#endif // EDITOR_RANGE_TEXT_REPL_H
