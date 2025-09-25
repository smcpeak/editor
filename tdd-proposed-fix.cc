// tdd-proposed-fix.cc
// Code for `tdd-proposed-fix` module.

#include "smbase/gdvalue-vector-fwd.h" // gdv::toGDValue(std::map)

#include "tdd-proposed-fix.h"          // this module

#include "smbase/compare-util.h"       // smbase::compare, RET_IF_COMPARE_MEMBERS
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/gdvalue-map.h"        // gdv::toGDValue(std::map)
#include "smbase/gdvalue-vector.h"     // gdv::toGDValue(std::map)

#include <sstream>                     // std::ostringstream
#include <vector>                      // std::vector [h]

using namespace smbase;


// --------------------------- TDD_TextEdit ----------------------------
// create-tuple-class: definitions for TDD_TextEdit
/*AUTO_CTC*/ TDD_TextEdit::TDD_TextEdit(
/*AUTO_CTC*/   TextMCoordRange const &range,
/*AUTO_CTC*/   std::string const &newText)
/*AUTO_CTC*/   : IMEMBFP(range),
/*AUTO_CTC*/     IMEMBFP(newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_TextEdit::TDD_TextEdit(
/*AUTO_CTC*/   TextMCoordRange &&range,
/*AUTO_CTC*/   std::string &&newText)
/*AUTO_CTC*/   : IMEMBMFP(range),
/*AUTO_CTC*/     IMEMBMFP(newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_TextEdit::TDD_TextEdit(TDD_TextEdit const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_range),
/*AUTO_CTC*/     DMEMB(m_newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_TextEdit::TDD_TextEdit(TDD_TextEdit &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_range),
/*AUTO_CTC*/     MDMEMB(m_newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_TextEdit &TDD_TextEdit::operator=(TDD_TextEdit const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/     CMEMB(m_newText);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_TextEdit &TDD_TextEdit::operator=(TDD_TextEdit &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_range);
/*AUTO_CTC*/     MCMEMB(m_newText);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(TDD_TextEdit const &a, TDD_TextEdit const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   // Remember to #include "smbase/compare-util.h" for these.
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_range);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_newText);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string TDD_TextEdit::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, TDD_TextEdit const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_TextEdit::operator gdv::GDValue() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   using namespace gdv;
/*AUTO_CTC*/   GDValue m(GDVK_TAGGED_ORDERED_MAP, "TDD_TextEdit"_sym);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_range);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_newText);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void TDD_TextEdit::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/


// -------------------------- TDD_ProposedFix --------------------------
// create-tuple-class: definitions for TDD_ProposedFix
/*AUTO_CTC*/ TDD_ProposedFix::TDD_ProposedFix(
/*AUTO_CTC*/   std::string const &title,
/*AUTO_CTC*/   std::map<std::string, stdfwd::vector<TDD_TextEdit>> const &changesForFile)
/*AUTO_CTC*/   : IMEMBFP(title),
/*AUTO_CTC*/     IMEMBFP(changesForFile)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_ProposedFix::TDD_ProposedFix(
/*AUTO_CTC*/   std::string &&title,
/*AUTO_CTC*/   std::map<std::string, stdfwd::vector<TDD_TextEdit>> &&changesForFile)
/*AUTO_CTC*/   : IMEMBMFP(title),
/*AUTO_CTC*/     IMEMBMFP(changesForFile)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_ProposedFix::TDD_ProposedFix(TDD_ProposedFix const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_title),
/*AUTO_CTC*/     DMEMB(m_changesForFile)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_ProposedFix::TDD_ProposedFix(TDD_ProposedFix &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_title),
/*AUTO_CTC*/     MDMEMB(m_changesForFile)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_ProposedFix &TDD_ProposedFix::operator=(TDD_ProposedFix const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_title);
/*AUTO_CTC*/     CMEMB(m_changesForFile);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_ProposedFix &TDD_ProposedFix::operator=(TDD_ProposedFix &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_title);
/*AUTO_CTC*/     MCMEMB(m_changesForFile);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(TDD_ProposedFix const &a, TDD_ProposedFix const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   // Remember to #include "smbase/compare-util.h" for these.
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_title);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_changesForFile);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string TDD_ProposedFix::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, TDD_ProposedFix const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ TDD_ProposedFix::operator gdv::GDValue() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   using namespace gdv;
/*AUTO_CTC*/   GDValue m(GDVK_TAGGED_ORDERED_MAP, "TDD_ProposedFix"_sym);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_title);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_changesForFile);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void TDD_ProposedFix::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/


// EOF
