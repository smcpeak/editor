// lsp-data.cc
// Code for `lsp-data.h`.

#include "lsp-data.h"                  // this module

#include "smbase/gdvalue-list.h"       // gdv::gdvTo<std::list>
#include "smbase/gdvalue-optional.h"   // gdv::gdvTo<std::optional>
#include "smbase/gdvalue-parser-ops.h" // gdv::GDValueParser
#include "smbase/gdvalue.h"            // gdv::GDValue

#include <optional>                    // std::optional


using namespace gdv;


// --------------------------- LSP_Position ----------------------------
// create-tuple-class: definitions for LSP_Position
/*AUTO_CTC*/ LSP_Position::LSP_Position(
/*AUTO_CTC*/   int line,
/*AUTO_CTC*/   int character)
/*AUTO_CTC*/   : m_line(line),
/*AUTO_CTC*/     m_character(character)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Position::LSP_Position(LSP_Position const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_line),
/*AUTO_CTC*/     DMEMB(m_character)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Position &LSP_Position::operator=(LSP_Position const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_line);
/*AUTO_CTC*/     CMEMB(m_character);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


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
{}


// ----------------------------- LSP_Range -----------------------------
// create-tuple-class: definitions for LSP_Range
/*AUTO_CTC*/ LSP_Range::LSP_Range(
/*AUTO_CTC*/   LSP_Position const &start,
/*AUTO_CTC*/   LSP_Position const &end)
/*AUTO_CTC*/   : m_start(start),
/*AUTO_CTC*/     m_end(end)
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


// --------------------------- LSP_Location ----------------------------
// create-tuple-class: definitions for LSP_Location
/*AUTO_CTC*/ LSP_Location::LSP_Location(
/*AUTO_CTC*/   std::string const &uri,
/*AUTO_CTC*/   LSP_Range const &range)
/*AUTO_CTC*/   : m_uri(uri),
/*AUTO_CTC*/     m_range(range)
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
/*AUTO_CTC*/   : m_location(location),
/*AUTO_CTC*/     m_message(message)
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
/*AUTO_CTC*/   : m_range(range),
/*AUTO_CTC*/     m_severity(severity),
/*AUTO_CTC*/     m_source(source),
/*AUTO_CTC*/     m_message(message),
/*AUTO_CTC*/     m_relatedInformation(relatedInformation)
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
/*AUTO_CTC*/   std::optional<int> const &version,
/*AUTO_CTC*/   std::list<LSP_Diagnostic> const &diagnostics)
/*AUTO_CTC*/   : m_uri(uri),
/*AUTO_CTC*/     m_version(version),
/*AUTO_CTC*/     m_diagnostics(diagnostics)
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


// EOF
