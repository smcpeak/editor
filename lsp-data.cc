// lsp-data.cc
// Code for `lsp-data.h`.

#include "lsp-data.h"                  // this module

#include "uri-util.h"                  // makeFileURI, getFileURIPath

#include "smbase/compare-util.h"       // RET_IF_COMPARE_MEMBERS, smbase::compare
#include "smbase/gdvalue-list.h"       // gdv::gdvTo<std::list>
#include "smbase/gdvalue-optional.h"   // gdv::gdvTo<std::optional>
#include "smbase/gdvalue-parser-ops.h" // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue

#include <optional>                    // std::optional
#include <list>                        // std::list
#include <sstream>                     // std::ostringstream


using namespace gdv;
using namespace smbase;


// --------------------------- LSP_Position ----------------------------
// create-tuple-class: definitions for LSP_Position
/*AUTO_CTC*/ LSP_Position::LSP_Position(
/*AUTO_CTC*/   LineIndex const &line,
/*AUTO_CTC*/   ByteIndex const &character)
/*AUTO_CTC*/   : IMEMBFP(line),
/*AUTO_CTC*/     IMEMBFP(character)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   selfCheck();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Position::LSP_Position(LSP_Position const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_line),
/*AUTO_CTC*/     DMEMB(m_character)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   selfCheck();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Position &LSP_Position::operator=(LSP_Position const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_line);
/*AUTO_CTC*/     CMEMB(m_character);
/*AUTO_CTC*/     selfCheck();
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(LSP_Position const &a, LSP_Position const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_line);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_character);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string LSP_Position::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void LSP_Position::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   os << "{";
/*AUTO_CTC*/   WRITE_MEMBER(m_line);
/*AUTO_CTC*/   WRITE_MEMBER(m_character);
/*AUTO_CTC*/   os << " }";
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, LSP_Position const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


void LSP_Position::selfCheck() const
{
  m_line.selfCheck();
  xassert(m_character >= 0);
}


LSP_Position::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_line);
  GDV_WRITE_MEMBER_STR(m_character);

  return m;
}


LSP_Position::LSP_Position(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_line),
    GDVP_READ_MEMBER_STR(m_character)
{
  selfCheck();
}


LSP_Position LSP_Position::plusCharacters(ByteDifference n) const
{
  return LSP_Position(m_line, m_character + n);
}


// ----------------------------- LSP_Range -----------------------------
// create-tuple-class: definitions for LSP_Range
/*AUTO_CTC*/ LSP_Range::LSP_Range(
/*AUTO_CTC*/   LSP_Position const &start,
/*AUTO_CTC*/   LSP_Position const &end)
/*AUTO_CTC*/   : IMEMBFP(start),
/*AUTO_CTC*/     IMEMBFP(end)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Range::LSP_Range(LSP_Range const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_start),
/*AUTO_CTC*/     DMEMB(m_end)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Range &LSP_Range::operator=(LSP_Range const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_start);
/*AUTO_CTC*/     CMEMB(m_end);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ int compare(LSP_Range const &a, LSP_Range const &b)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_start);
/*AUTO_CTC*/   RET_IF_COMPARE_MEMBERS(m_end);
/*AUTO_CTC*/   return 0;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::string LSP_Range::toString() const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   std::ostringstream oss;
/*AUTO_CTC*/   write(oss);
/*AUTO_CTC*/   return oss.str();
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ void LSP_Range::write(std::ostream &os) const
/*AUTO_CTC*/ {
/*AUTO_CTC*/   os << "{";
/*AUTO_CTC*/   WRITE_MEMBER(m_start);
/*AUTO_CTC*/   WRITE_MEMBER(m_end);
/*AUTO_CTC*/   os << " }";
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ std::ostream &operator<<(std::ostream &os, LSP_Range const &obj)
/*AUTO_CTC*/ {
/*AUTO_CTC*/   obj.write(os);
/*AUTO_CTC*/   return os;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_Range::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_start);
  GDV_WRITE_MEMBER_STR(m_end);

  return m;
}


LSP_Range::LSP_Range(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_start),
    GDVP_READ_MEMBER_STR(m_end)
{}


// -------------------------- LSP_FilenameURI --------------------------
// create-tuple-class: definitions for LSP_FilenameURI
/*AUTO_CTC*/ LSP_FilenameURI::LSP_FilenameURI(
/*AUTO_CTC*/   std::string const &innerUri)
/*AUTO_CTC*/   : IMEMBFP(innerUri)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_FilenameURI::LSP_FilenameURI(LSP_FilenameURI const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_innerUri)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_FilenameURI &LSP_FilenameURI::operator=(LSP_FilenameURI const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_innerUri);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_FilenameURI::operator gdv::GDValue() const
{
  return GDValue(m_innerUri);
}


LSP_FilenameURI::LSP_FilenameURI(gdv::GDValueParser const &p)
  : m_innerUri(gdvpTo<std::string>(p))
{}


/*static*/ LSP_FilenameURI LSP_FilenameURI::fromFname(
  std::string const &fname)
{
  return LSP_FilenameURI(makeFileURI(fname));
}


std::string LSP_FilenameURI::getFname() const
{
  return getFileURIPath(m_innerUri);
}


// --------------------------- LSP_Location ----------------------------
// create-tuple-class: definitions for LSP_Location
/*AUTO_CTC*/ LSP_Location::LSP_Location(
/*AUTO_CTC*/   LSP_FilenameURI const &uri,
/*AUTO_CTC*/   LSP_Range const &range)
/*AUTO_CTC*/   : IMEMBFP(uri),
/*AUTO_CTC*/     IMEMBFP(range)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Location::LSP_Location(LSP_Location const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_uri),
/*AUTO_CTC*/     DMEMB(m_range)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Location &LSP_Location::operator=(LSP_Location const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_uri);
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_Location::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_uri);
  GDV_WRITE_MEMBER_STR(m_range);

  return m;
}


LSP_Location::LSP_Location(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_uri),
    GDVP_READ_MEMBER_STR(m_range)
{}


// ----------------- LSP_DiagnosticRelatedInformation ------------------
// create-tuple-class: definitions for LSP_DiagnosticRelatedInformation
/*AUTO_CTC*/ LSP_DiagnosticRelatedInformation::LSP_DiagnosticRelatedInformation(
/*AUTO_CTC*/   LSP_Location const &location,
/*AUTO_CTC*/   std::string const &message)
/*AUTO_CTC*/   : IMEMBFP(location),
/*AUTO_CTC*/     IMEMBFP(message)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DiagnosticRelatedInformation::LSP_DiagnosticRelatedInformation(LSP_DiagnosticRelatedInformation const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_location),
/*AUTO_CTC*/     DMEMB(m_message)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DiagnosticRelatedInformation &LSP_DiagnosticRelatedInformation::operator=(LSP_DiagnosticRelatedInformation const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_location);
/*AUTO_CTC*/     CMEMB(m_message);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_DiagnosticRelatedInformation::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_location);
  GDV_WRITE_MEMBER_STR(m_message);

  return m;
}


LSP_DiagnosticRelatedInformation::LSP_DiagnosticRelatedInformation(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_location),
    GDVP_READ_MEMBER_STR(m_message)
{}


// -------------------------- LSP_Diagnostic ---------------------------
// create-tuple-class: definitions for LSP_Diagnostic
/*AUTO_CTC*/ LSP_Diagnostic::LSP_Diagnostic(
/*AUTO_CTC*/   LSP_Range const &range,
/*AUTO_CTC*/   int severity,
/*AUTO_CTC*/   std::optional<std::string> const &source,
/*AUTO_CTC*/   std::string const &message,
/*AUTO_CTC*/   std::list<LSP_DiagnosticRelatedInformation> const &relatedInformation)
/*AUTO_CTC*/   : IMEMBFP(range),
/*AUTO_CTC*/     IMEMBFP(severity),
/*AUTO_CTC*/     IMEMBFP(source),
/*AUTO_CTC*/     IMEMBFP(message),
/*AUTO_CTC*/     IMEMBFP(relatedInformation)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Diagnostic::LSP_Diagnostic(LSP_Diagnostic const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_range),
/*AUTO_CTC*/     DMEMB(m_severity),
/*AUTO_CTC*/     DMEMB(m_source),
/*AUTO_CTC*/     DMEMB(m_message),
/*AUTO_CTC*/     DMEMB(m_relatedInformation)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Diagnostic &LSP_Diagnostic::operator=(LSP_Diagnostic const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/     CMEMB(m_severity);
/*AUTO_CTC*/     CMEMB(m_source);
/*AUTO_CTC*/     CMEMB(m_message);
/*AUTO_CTC*/     CMEMB(m_relatedInformation);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_Diagnostic::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_range);
  GDV_WRITE_MEMBER_STR(m_severity);
  GDV_WRITE_MEMBER_STR(m_source);
  GDV_WRITE_MEMBER_STR(m_message);
  GDV_WRITE_MEMBER_STR(m_relatedInformation);

  return m;
}


LSP_Diagnostic::LSP_Diagnostic(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR    (m_range),
    GDVP_READ_OPT_MEMBER_STR(m_severity),
    GDVP_READ_OPT_MEMBER_STR(m_source),
    GDVP_READ_MEMBER_STR    (m_message),
    GDVP_READ_OPT_MEMBER_STR(m_relatedInformation)
{
  // The LSP spec says an omitted `severity` should be treated as Error.
  if (!p.mapContains("severity")) {
    m_severity = 1;
  }
}


// ------------------- LSP_PublishDiagnosticsParams --------------------
// create-tuple-class: definitions for LSP_PublishDiagnosticsParams
/*AUTO_CTC*/ LSP_PublishDiagnosticsParams::LSP_PublishDiagnosticsParams(
/*AUTO_CTC*/   std::string const &uri,
/*AUTO_CTC*/   std::optional<LSP_VersionNumber> const &version,
/*AUTO_CTC*/   std::list<LSP_Diagnostic> const &diagnostics)
/*AUTO_CTC*/   : IMEMBFP(uri),
/*AUTO_CTC*/     IMEMBFP(version),
/*AUTO_CTC*/     IMEMBFP(diagnostics)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_PublishDiagnosticsParams::LSP_PublishDiagnosticsParams(LSP_PublishDiagnosticsParams const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_uri),
/*AUTO_CTC*/     DMEMB(m_version),
/*AUTO_CTC*/     DMEMB(m_diagnostics)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_PublishDiagnosticsParams &LSP_PublishDiagnosticsParams::operator=(LSP_PublishDiagnosticsParams const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_uri);
/*AUTO_CTC*/     CMEMB(m_version);
/*AUTO_CTC*/     CMEMB(m_diagnostics);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_PublishDiagnosticsParams::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_uri);
  GDV_WRITE_MEMBER_STR(m_version);
  GDV_WRITE_MEMBER_STR(m_diagnostics);

  return m;
}


LSP_PublishDiagnosticsParams::LSP_PublishDiagnosticsParams(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_uri),
    GDVP_READ_OPT_MEMBER_STR(m_version),
    GDVP_READ_MEMBER_STR(m_diagnostics)
{}


// ----------------------- LSP_LocationSequence ------------------------
// create-tuple-class: definitions for LSP_LocationSequence
/*AUTO_CTC*/ LSP_LocationSequence::LSP_LocationSequence(
/*AUTO_CTC*/   std::list<LSP_Location> const &locations)
/*AUTO_CTC*/   : IMEMBFP(locations)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_LocationSequence::LSP_LocationSequence(
/*AUTO_CTC*/   std::list<LSP_Location> &&locations)
/*AUTO_CTC*/   : IMEMBMFP(locations)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_LocationSequence::LSP_LocationSequence(LSP_LocationSequence const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_locations)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_LocationSequence::LSP_LocationSequence(LSP_LocationSequence &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_locations)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_LocationSequence &LSP_LocationSequence::operator=(LSP_LocationSequence const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_locations);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_LocationSequence &LSP_LocationSequence::operator=(LSP_LocationSequence &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_locations);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_LocationSequence::operator gdv::GDValue() const
{
  return toGDValue(m_locations);
}


LSP_LocationSequence::LSP_LocationSequence(gdv::GDValueParser const &p)
    // The spec lists some other possibilities, including a Location not
    // wrapped in an array container, and `null`, but `clangd` appears
    // to always provide the container.
  : m_locations(gdvpTo<std::list<LSP_Location>>(p))
{}


// --------------------------- LSP_TextEdit ----------------------------
// create-tuple-class: definitions for LSP_TextEdit
/*AUTO_CTC*/ LSP_TextEdit::LSP_TextEdit(
/*AUTO_CTC*/   LSP_Range const &range,
/*AUTO_CTC*/   std::string const &newText)
/*AUTO_CTC*/   : IMEMBFP(range),
/*AUTO_CTC*/     IMEMBFP(newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextEdit::LSP_TextEdit(
/*AUTO_CTC*/   LSP_Range &&range,
/*AUTO_CTC*/   std::string &&newText)
/*AUTO_CTC*/   : IMEMBMFP(range),
/*AUTO_CTC*/     IMEMBMFP(newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextEdit::LSP_TextEdit(LSP_TextEdit const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_range),
/*AUTO_CTC*/     DMEMB(m_newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextEdit::LSP_TextEdit(LSP_TextEdit &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_range),
/*AUTO_CTC*/     MDMEMB(m_newText)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextEdit &LSP_TextEdit::operator=(LSP_TextEdit const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/     CMEMB(m_newText);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextEdit &LSP_TextEdit::operator=(LSP_TextEdit &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_range);
/*AUTO_CTC*/     MCMEMB(m_newText);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_TextEdit::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_range);
  GDV_WRITE_MEMBER_STR(m_newText);

  return m;
}


LSP_TextEdit::LSP_TextEdit(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_range),
    GDVP_READ_MEMBER_STR(m_newText)
{}


// ------------------------ LSP_CompletionItem -------------------------
// create-tuple-class: definitions for LSP_CompletionItem
/*AUTO_CTC*/ LSP_CompletionItem::LSP_CompletionItem(
/*AUTO_CTC*/   std::string const &label,
/*AUTO_CTC*/   LSP_TextEdit const &textEdit)
/*AUTO_CTC*/   : IMEMBFP(label),
/*AUTO_CTC*/     IMEMBFP(textEdit)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionItem::LSP_CompletionItem(
/*AUTO_CTC*/   std::string &&label,
/*AUTO_CTC*/   LSP_TextEdit &&textEdit)
/*AUTO_CTC*/   : IMEMBMFP(label),
/*AUTO_CTC*/     IMEMBMFP(textEdit)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionItem::LSP_CompletionItem(LSP_CompletionItem const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_label),
/*AUTO_CTC*/     DMEMB(m_textEdit)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionItem::LSP_CompletionItem(LSP_CompletionItem &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_label),
/*AUTO_CTC*/     MDMEMB(m_textEdit)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionItem &LSP_CompletionItem::operator=(LSP_CompletionItem const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_label);
/*AUTO_CTC*/     CMEMB(m_textEdit);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionItem &LSP_CompletionItem::operator=(LSP_CompletionItem &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_label);
/*AUTO_CTC*/     MCMEMB(m_textEdit);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_CompletionItem::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_label);
  GDV_WRITE_MEMBER_STR(m_textEdit);

  return m;
}


LSP_CompletionItem::LSP_CompletionItem(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_label),
    GDVP_READ_MEMBER_STR(m_textEdit)
{}


// ------------------------ LSP_CompletionList -------------------------
// create-tuple-class: definitions for LSP_CompletionList
/*AUTO_CTC*/ LSP_CompletionList::LSP_CompletionList(
/*AUTO_CTC*/   bool isIncomplete,
/*AUTO_CTC*/   std::list<LSP_CompletionItem> const &items)
/*AUTO_CTC*/   : IMEMBFP(isIncomplete),
/*AUTO_CTC*/     IMEMBFP(items)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionList::LSP_CompletionList(
/*AUTO_CTC*/   bool isIncomplete,
/*AUTO_CTC*/   std::list<LSP_CompletionItem> &&items)
/*AUTO_CTC*/   : IMEMBMFP(isIncomplete),
/*AUTO_CTC*/     IMEMBMFP(items)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionList::LSP_CompletionList(LSP_CompletionList const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_isIncomplete),
/*AUTO_CTC*/     DMEMB(m_items)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionList::LSP_CompletionList(LSP_CompletionList &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_isIncomplete),
/*AUTO_CTC*/     MDMEMB(m_items)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionList &LSP_CompletionList::operator=(LSP_CompletionList const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_isIncomplete);
/*AUTO_CTC*/     CMEMB(m_items);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_CompletionList &LSP_CompletionList::operator=(LSP_CompletionList &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_isIncomplete);
/*AUTO_CTC*/     MCMEMB(m_items);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_CompletionList::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_isIncomplete);
  GDV_WRITE_MEMBER_STR(m_items);

  return m;
}


LSP_CompletionList::LSP_CompletionList(gdv::GDValueParser const &p)
  : GDVP_READ_MEMBER_STR(m_isIncomplete),
    GDVP_READ_MEMBER_STR(m_items)
{}


// -------------------- LSP_TextDocumentIdentifier ---------------------
// create-tuple-class: definitions for LSP_TextDocumentIdentifier
/*AUTO_CTC*/ LSP_TextDocumentIdentifier::LSP_TextDocumentIdentifier(
/*AUTO_CTC*/   LSP_FilenameURI const &uri)
/*AUTO_CTC*/   : IMEMBFP(uri)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentIdentifier::LSP_TextDocumentIdentifier(
/*AUTO_CTC*/   LSP_FilenameURI &&uri)
/*AUTO_CTC*/   : IMEMBMFP(uri)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentIdentifier::LSP_TextDocumentIdentifier(LSP_TextDocumentIdentifier const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_uri)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentIdentifier::LSP_TextDocumentIdentifier(LSP_TextDocumentIdentifier &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_uri)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentIdentifier &LSP_TextDocumentIdentifier::operator=(LSP_TextDocumentIdentifier const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_uri);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentIdentifier &LSP_TextDocumentIdentifier::operator=(LSP_TextDocumentIdentifier &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_uri);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_TextDocumentIdentifier::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_uri);

  return m;
}


/*static*/ LSP_TextDocumentIdentifier
LSP_TextDocumentIdentifier::fromFname(
  std::string const &fname)
{
  return LSP_TextDocumentIdentifier(LSP_FilenameURI::fromFname(fname));
}


// ---------------- LSP_VersionedTextDocumentIdentifier ----------------
// create-tuple-class: definitions for LSP_VersionedTextDocumentIdentifier
/*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier::LSP_VersionedTextDocumentIdentifier(
/*AUTO_CTC*/   LSP_FilenameURI const &uri,
/*AUTO_CTC*/   LSP_VersionNumber const &version)
/*AUTO_CTC*/   : IMEMBFP(uri),
/*AUTO_CTC*/     IMEMBFP(version)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier::LSP_VersionedTextDocumentIdentifier(
/*AUTO_CTC*/   LSP_FilenameURI &&uri,
/*AUTO_CTC*/   LSP_VersionNumber &&version)
/*AUTO_CTC*/   : IMEMBMFP(uri),
/*AUTO_CTC*/     IMEMBMFP(version)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier::LSP_VersionedTextDocumentIdentifier(LSP_VersionedTextDocumentIdentifier const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_uri),
/*AUTO_CTC*/     DMEMB(m_version)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier::LSP_VersionedTextDocumentIdentifier(LSP_VersionedTextDocumentIdentifier &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_uri),
/*AUTO_CTC*/     MDMEMB(m_version)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier &LSP_VersionedTextDocumentIdentifier::operator=(LSP_VersionedTextDocumentIdentifier const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_uri);
/*AUTO_CTC*/     CMEMB(m_version);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier &LSP_VersionedTextDocumentIdentifier::operator=(LSP_VersionedTextDocumentIdentifier &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_uri);
/*AUTO_CTC*/     MCMEMB(m_version);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_VersionedTextDocumentIdentifier::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_uri);
  GDV_WRITE_MEMBER_STR(m_version);

  return m;
}


/*static*/ LSP_VersionedTextDocumentIdentifier
LSP_VersionedTextDocumentIdentifier::fromFname(
  std::string const &fname,
  LSP_VersionNumber version)
{
  return LSP_VersionedTextDocumentIdentifier(
    LSP_FilenameURI::fromFname(fname), version);
}


// ---------------- LSP_TextDocumentContentChangeEvent -----------------
// create-tuple-class: definitions for LSP_TextDocumentContentChangeEvent
/*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent::LSP_TextDocumentContentChangeEvent(
/*AUTO_CTC*/   std::optional<LSP_Range> const &range,
/*AUTO_CTC*/   std::string const &text)
/*AUTO_CTC*/   : IMEMBFP(range),
/*AUTO_CTC*/     IMEMBFP(text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent::LSP_TextDocumentContentChangeEvent(
/*AUTO_CTC*/   std::optional<LSP_Range> &&range,
/*AUTO_CTC*/   std::string &&text)
/*AUTO_CTC*/   : IMEMBMFP(range),
/*AUTO_CTC*/     IMEMBMFP(text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent::LSP_TextDocumentContentChangeEvent(LSP_TextDocumentContentChangeEvent const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_range),
/*AUTO_CTC*/     DMEMB(m_text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent::LSP_TextDocumentContentChangeEvent(LSP_TextDocumentContentChangeEvent &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_range),
/*AUTO_CTC*/     MDMEMB(m_text)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent &LSP_TextDocumentContentChangeEvent::operator=(LSP_TextDocumentContentChangeEvent const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/     CMEMB(m_text);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent &LSP_TextDocumentContentChangeEvent::operator=(LSP_TextDocumentContentChangeEvent &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_range);
/*AUTO_CTC*/     MCMEMB(m_text);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_TextDocumentContentChangeEvent::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_range);
  GDV_WRITE_MEMBER_STR(m_text);

  return m;
}


// ------------------ LSP_DidChangeTextDocumentParams ------------------
// create-tuple-class: definitions for LSP_DidChangeTextDocumentParams
/*AUTO_CTC*/ LSP_DidChangeTextDocumentParams::LSP_DidChangeTextDocumentParams(
/*AUTO_CTC*/   LSP_VersionedTextDocumentIdentifier const &textDocument,
/*AUTO_CTC*/   std::list<LSP_TextDocumentContentChangeEvent> const &contentChanges,
/*AUTO_CTC*/   std::optional<bool> const &wantDiagnostics)
/*AUTO_CTC*/   : IMEMBFP(textDocument),
/*AUTO_CTC*/     IMEMBFP(contentChanges),
/*AUTO_CTC*/     IMEMBFP(wantDiagnostics)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DidChangeTextDocumentParams::LSP_DidChangeTextDocumentParams(
/*AUTO_CTC*/   LSP_VersionedTextDocumentIdentifier &&textDocument,
/*AUTO_CTC*/   std::list<LSP_TextDocumentContentChangeEvent> &&contentChanges,
/*AUTO_CTC*/   std::optional<bool> &&wantDiagnostics)
/*AUTO_CTC*/   : IMEMBMFP(textDocument),
/*AUTO_CTC*/     IMEMBMFP(contentChanges),
/*AUTO_CTC*/     IMEMBMFP(wantDiagnostics)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DidChangeTextDocumentParams::LSP_DidChangeTextDocumentParams(LSP_DidChangeTextDocumentParams const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_textDocument),
/*AUTO_CTC*/     DMEMB(m_contentChanges),
/*AUTO_CTC*/     DMEMB(m_wantDiagnostics)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DidChangeTextDocumentParams::LSP_DidChangeTextDocumentParams(LSP_DidChangeTextDocumentParams &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_textDocument),
/*AUTO_CTC*/     MDMEMB(m_contentChanges),
/*AUTO_CTC*/     MDMEMB(m_wantDiagnostics)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DidChangeTextDocumentParams &LSP_DidChangeTextDocumentParams::operator=(LSP_DidChangeTextDocumentParams const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_textDocument);
/*AUTO_CTC*/     CMEMB(m_contentChanges);
/*AUTO_CTC*/     CMEMB(m_wantDiagnostics);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_DidChangeTextDocumentParams &LSP_DidChangeTextDocumentParams::operator=(LSP_DidChangeTextDocumentParams &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_textDocument);
/*AUTO_CTC*/     MCMEMB(m_contentChanges);
/*AUTO_CTC*/     MCMEMB(m_wantDiagnostics);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_DidChangeTextDocumentParams::LSP_DidChangeTextDocumentParams(
  LSP_VersionedTextDocumentIdentifier const &textDocument,
  std::list<LSP_TextDocumentContentChangeEvent> const &contentChanges)
  : IMEMBFP(textDocument),
    IMEMBFP(contentChanges),
    m_wantDiagnostics()
{}


LSP_DidChangeTextDocumentParams::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_textDocument);
  GDV_WRITE_MEMBER_STR(m_contentChanges);
  if (m_wantDiagnostics) {
    GDV_WRITE_MEMBER_STR(m_wantDiagnostics);
  }

  return m;
}


std::string LSP_DidChangeTextDocumentParams::getFname() const
{
  return m_textDocument.getFname();
}


// ------------------ LSP_TextDocumentPositionParams -------------------
// create-tuple-class: definitions for LSP_TextDocumentPositionParams
/*AUTO_CTC*/ LSP_TextDocumentPositionParams::LSP_TextDocumentPositionParams(
/*AUTO_CTC*/   LSP_TextDocumentIdentifier const &textDocument,
/*AUTO_CTC*/   LSP_Position const &position)
/*AUTO_CTC*/   : IMEMBFP(textDocument),
/*AUTO_CTC*/     IMEMBFP(position)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentPositionParams::LSP_TextDocumentPositionParams(
/*AUTO_CTC*/   LSP_TextDocumentIdentifier &&textDocument,
/*AUTO_CTC*/   LSP_Position &&position)
/*AUTO_CTC*/   : IMEMBMFP(textDocument),
/*AUTO_CTC*/     IMEMBMFP(position)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentPositionParams::LSP_TextDocumentPositionParams(LSP_TextDocumentPositionParams const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_textDocument),
/*AUTO_CTC*/     DMEMB(m_position)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentPositionParams::LSP_TextDocumentPositionParams(LSP_TextDocumentPositionParams &&obj) noexcept
/*AUTO_CTC*/   : MDMEMB(m_textDocument),
/*AUTO_CTC*/     MDMEMB(m_position)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentPositionParams &LSP_TextDocumentPositionParams::operator=(LSP_TextDocumentPositionParams const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_textDocument);
/*AUTO_CTC*/     CMEMB(m_position);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_TextDocumentPositionParams &LSP_TextDocumentPositionParams::operator=(LSP_TextDocumentPositionParams &&obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     MCMEMB(m_textDocument);
/*AUTO_CTC*/     MCMEMB(m_position);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_TextDocumentPositionParams::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  GDV_WRITE_MEMBER_STR(m_textDocument);
  GDV_WRITE_MEMBER_STR(m_position);

  return m;
}


std::string LSP_TextDocumentPositionParams::getFname() const
{
  return m_textDocument.getFname();
}


// EOF
