// range-text-repl.cc
// Code for `range-text-repl` module.

#include "range-text-repl.h"           // this module

#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-macros.h"          // IMEMBFP, IMEMBMFP, MDMEMB, MCMEMB

#include <optional>                    // std::optional
#include <sstream>                     // std::ostringstream
#include <string>                      // std::string


// create-tuple-class: definitions for RangeTextReplacement
/*AUTO_CTC*/ RangeTextReplacement::RangeTextReplacement(
/*AUTO_CTC*/   std::optional<TextMCoordRange> const &range,
/*AUTO_CTC*/   std::string const &text)
/*AUTO_CTC*/   : IMEMBFP(range),
/*AUTO_CTC*/     IMEMBFP(text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement::RangeTextReplacement(
/*AUTO_CTC*/   std::optional<TextMCoordRange> &&range,
/*AUTO_CTC*/   std::string &&text)
/*AUTO_CTC*/   : IMEMBMFP(range),
/*AUTO_CTC*/     IMEMBMFP(text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement::RangeTextReplacement(RangeTextReplacement const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_range),
/*AUTO_CTC*/     DMEMB(m_text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement::RangeTextReplacement(RangeTextReplacement &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_range),
/*AUTO_CTC*/     MDMEMB(m_text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement &RangeTextReplacement::operator=(RangeTextReplacement const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/     CMEMB(m_text);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement &RangeTextReplacement::operator=(RangeTextReplacement &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_range);
/*AUTO_CTC*/     MCMEMB(m_text);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string RangeTextReplacement::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, RangeTextReplacement const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement::operator gdv::GDValue() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   using namespace gdv;
/*AUTO_CTC*/   GDValue m(GDVK_TAGGED_ORDERED_MAP, "RangeTextReplacement"_sym);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_range);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_text);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void RangeTextReplacement::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ RangeTextReplacement::RangeTextReplacement(gdv::GDValueParser const &p)
/*AUTO_CTC*/   : GDVP_READ_MEMBER_SYM(m_range),
/*AUTO_CTC*/     GDVP_READ_MEMBER_SYM(m_text)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   p.checkTaggedOrderedMapTag("RangeTextReplacement");
/*AUTO_CTC*/ }
/*AUTO_CTC*/


std::string toString(RangeTextReplacement const &obj)
{
  return obj.toString();
}


// EOF
