// host-file-line.cc
// Code for `host-file-line` module.

#include "host-file-line.h"            // this module

#include "smbase/compare-util.h"       // RET_IF_COMPARE_MEMBERS
#include "smbase/gdvalue.h"            // gdv::GDValue

using namespace smbase;


// ---- create-tuple-class: definitions for HostFileLine
/*AUTO_CTC*/ HostFileLine::HostFileLine(
/*AUTO_CTC*/   HostAndResourceName const &harn,
/*AUTO_CTC*/   LineIndex const &lineIndex)
/*AUTO_CTC*/   : IMEMBFP(harn),
/*AUTO_CTC*/     IMEMBFP(lineIndex)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ HostFileLine::HostFileLine(HostFileLine const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_harn),
/*AUTO_CTC*/     DMEMB(m_lineIndex)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ HostFileLine &HostFileLine::operator=(HostFileLine const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_harn);
/*AUTO_CTC*/     CMEMB(m_lineIndex);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(HostFileLine const &a, HostFileLine const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   // Remember to #include "smbase/compare-util.h" for these.
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_harn);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_lineIndex);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string HostFileLine::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, HostFileLine const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ HostFileLine::operator gdv::GDValue() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   using namespace gdv;
/*AUTO_CTC*/   GDValue m(GDVK_TAGGED_ORDERED_MAP, "HostFileLine"_sym);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_harn);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_lineIndex);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void HostFileLine::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/


// EOF
