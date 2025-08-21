// range-text-repl.cc
// Code for `range-text-repl` module.

#include "range-text-repl.h"           // this module

#include "smbase/sm-macros.h"          // IMEMBFP, IMEMBMFP, MDMEMB, MCMEMB


RangeTextReplacement::~RangeTextReplacement()
{}


RangeTextReplacement::RangeTextReplacement(
  std::optional<TextMCoordRange> range,
  std::string const &text)
  : IMEMBFP(range),
    IMEMBFP(text)
{}


RangeTextReplacement::RangeTextReplacement(
  std::optional<TextMCoordRange> range,
  std::string &&text)
  : IMEMBFP(range),
    IMEMBMFP(text)
{}


RangeTextReplacement::RangeTextReplacement(RangeTextReplacement &&obj)
  : MDMEMB(m_range),
    MDMEMB(m_text)
{}


RangeTextReplacement &RangeTextReplacement::operator=(RangeTextReplacement &&obj)
{
  if (this != &obj) {
    MCMEMB(m_range);
    MCMEMB(m_text);
  }
  return *this;
}


// EOF
