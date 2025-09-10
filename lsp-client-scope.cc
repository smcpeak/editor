// lsp-client-scope.cc
// Code for `lsp-client-scope` module.

#include "lsp-client-scope.h"          // this module

#include "doc-type.h"                  // DocumentType
#include "host-name.h"                 // HostName
#include "named-td.h"                  // NamedTextDocument

#include "smbase/compare-util.h"       // RET_IF_COMPARE_MEMBERS, smbase::compare
#include "smbase/gdvalue.h"            // gdv::GDValue

using namespace smbase;


// ---- create-tuple-class: definitions for LSPClientScope
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(
/*AUTO_CTC*/   HostName const &hostName,
/*AUTO_CTC*/   DocumentType const &documentType)
/*AUTO_CTC*/   : IMEMBFP(hostName),
/*AUTO_CTC*/     IMEMBFP(documentType)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(
/*AUTO_CTC*/   HostName &&hostName,
/*AUTO_CTC*/   DocumentType &&documentType)
/*AUTO_CTC*/   : IMEMBMFP(hostName),
/*AUTO_CTC*/     IMEMBMFP(documentType)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(LSPClientScope const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_hostName),
/*AUTO_CTC*/     DMEMB(m_documentType)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(LSPClientScope &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_hostName),
/*AUTO_CTC*/     MDMEMB(m_documentType)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope &LSPClientScope::operator=(LSPClientScope const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_hostName);
/*AUTO_CTC*/     CMEMB(m_documentType);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope &LSPClientScope::operator=(LSPClientScope &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_hostName);
/*AUTO_CTC*/     MCMEMB(m_documentType);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(LSPClientScope const &a, LSPClientScope const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_hostName);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_documentType);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string LSPClientScope::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, LSPClientScope const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::operator gdv::GDValue() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   using namespace gdv;
/*AUTO_CTC*/   GDValue m(GDVK_TAGGED_ORDERED_MAP, "LSPClientScope"_sym);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_hostName);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_documentType);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void LSPClientScope::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/


/*static*/ LSPClientScope LSPClientScope::forNTD(
  NamedTextDocument const *ntd)
{
  return LSPClientScope(ntd->hostName(), ntd->documentType());
}


std::string LSPClientScope::languageName() const
{
  return ::languageName(m_documentType);
}


std::string LSPClientScope::hostString() const
{
  return m_hostName.toString();
}


std::string LSPClientScope::description() const
{
  return stringb(languageName() << " files on " <<
                 hostString() << " host");
}


// EOF
