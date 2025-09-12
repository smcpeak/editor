// lsp-client-scope.cc
// Code for `lsp-client-scope` module.

#include "lsp-client-scope.h"          // this module

#include "doc-type.h"                  // DocumentType
#include "host-name.h"                 // HostName
#include "lsp-conv.h"                  // lspLanguageIdForDT
#include "named-td.h"                  // NamedTextDocument

#include "smbase/compare-util.h"       // RET_IF_COMPARE_MEMBERS, smbase::compare
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // doubleQuote, replaceNonAlnumWith

#include <sstream>                     // std::ostringstream

using namespace smbase;


// ---- create-tuple-class: definitions for LSPClientScope
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(
/*AUTO_CTC*/   HostName const &hostName,
/*AUTO_CTC*/   std::optional<std::string> const &directory,
/*AUTO_CTC*/   DocumentType const &documentType)
/*AUTO_CTC*/   : IMEMBFP(hostName),
/*AUTO_CTC*/     IMEMBFP(directory),
/*AUTO_CTC*/     IMEMBFP(documentType)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   selfCheck();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(
/*AUTO_CTC*/   HostName &&hostName,
/*AUTO_CTC*/   std::optional<std::string> &&directory,
/*AUTO_CTC*/   DocumentType &&documentType)
/*AUTO_CTC*/   : IMEMBMFP(hostName),
/*AUTO_CTC*/     IMEMBMFP(directory),
/*AUTO_CTC*/     IMEMBMFP(documentType)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   selfCheck();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(LSPClientScope const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_hostName),
/*AUTO_CTC*/     DMEMB(m_directory),
/*AUTO_CTC*/     DMEMB(m_documentType)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   selfCheck();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope::LSPClientScope(LSPClientScope &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_hostName),
/*AUTO_CTC*/     MDMEMB(m_directory),
/*AUTO_CTC*/     MDMEMB(m_documentType)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   selfCheck();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope &LSPClientScope::operator=(LSPClientScope const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_hostName);
/*AUTO_CTC*/     CMEMB(m_directory);
/*AUTO_CTC*/     CMEMB(m_documentType);
/*AUTO_CTC*/     selfCheck();
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSPClientScope &LSPClientScope::operator=(LSPClientScope &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_hostName);
/*AUTO_CTC*/     MCMEMB(m_directory);
/*AUTO_CTC*/     MCMEMB(m_documentType);
/*AUTO_CTC*/     selfCheck();
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(LSPClientScope const &a, LSPClientScope const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_hostName);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_directory);
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
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_directory);
/*AUTO_CTC*/   GDV_WRITE_MEMBER_SYM(m_documentType);
/*AUTO_CTC*/   return m;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void LSPClientScope::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   operator gdv::GDValue().writeIndented(os);
/*AUTO_CTC*/ }
/*AUTO_CTC*/


void LSPClientScope::selfCheck() const
{
  if (m_directory) {
    xassert(SMFileUtil().endsWithDirectorySeparator(*m_directory));
  }
}


/*static*/ LSPClientScope LSPClientScope::forNTD(
  NamedTextDocument const *ntd)
{
  DocumentType dt = ntd->documentType();

  if (dt == DocumentType::DT_PYTHON) {
    // The Python LSP server, `pylsp`, cannot find imported modules in
    // the same directory unless it is started in the same directory as
    // the file it is analyzing.  (Even setting `PYTHONPATH` does not
    // seem to help, although that wouldn't avoid needing to start a
    // separate process per directory.)
    return LSPClientScope(
      ntd->hostName(),
      ntd->documentName().directory(),
      dt);
  }
  else {
    // `clangd` seems to work fine with a single server handling files
    // scattered all over, across multiple projects.
    return LSPClientScope(
      ntd->hostName(),
      std::nullopt,
      dt);
  }
}


/*static*/ LSPClientScope LSPClientScope::localCPP()
{
  return LSPClientScope(
    HostName::asLocal(),
    std::nullopt,
    DocumentType::DT_CPP);
}


std::string LSPClientScope::hostString() const
{
  return m_hostName.toString();
}


bool LSPClientScope::hasDirectory() const
{
  return m_directory.has_value();
}


std::string LSPClientScope::directory() const
{
  xassertPrecondition(hasDirectory());
  return *m_directory;
}


std::string LSPClientScope::directoryFinalName() const
{
  xassertPrecondition(hasDirectory());

  SMFileUtil sfu;
  return sfu.splitPathBase(
    sfu.stripTrailingDirectorySeparator(*m_directory));
}


std::string LSPClientScope::languageName() const
{
  return ::languageName(m_documentType);
}


std::string LSPClientScope::description() const
{
  if (hasDirectory()) {
    return stringb(
      languageName() << " files on " <<
      hostString() << " host and in directory " <<
      doubleQuote(directory()));
  }
  else {
    return stringb(
      languageName() << " files on " <<
      hostString() << " host");
  }
}


std::string LSPClientScope::semiUniqueIDString() const
{
  std::ostringstream oss;

  oss << replaceNonAlnumWith(m_hostName.toString(), '-') << "-";

  if (hasDirectory()) {
    oss << directoryFinalName() << "-";
  }

  oss << lspLanguageIdForDT(m_documentType);

  return oss.str();
}


// EOF
