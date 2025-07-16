// lsp-data.cc
// Code for `lsp-data.h`.

#include "lsp-data.h"                  // this module

#include "smbase/gdvalue-list.h"       // gdv::gdvTo<std::list>
#include "smbase/gdvalue-parse.h"      // gdv::checkIsMap, etc.
#include "smbase/gdvalue.h"            // gdv::GDValue


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


LSP_Position::LSP_Position(gdv::GDValue const &m)
  : GDV_READ_MEMBER_SK(m_line),
    GDV_READ_MEMBER_SK(m_character)
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


LSP_Range::LSP_Range(gdv::GDValue const &m)
  : GDV_READ_MEMBER_SK(m_start),
    GDV_READ_MEMBER_SK(m_end)
{}


// -------------------------- LSP_Diagnostic ---------------------------
// create-tuple-class: definitions for LSP_Diagnostic
/*AUTO_CTC*/ LSP_Diagnostic::LSP_Diagnostic(
/*AUTO_CTC*/   LSP_Range const &range,
/*AUTO_CTC*/   int severity,
/*AUTO_CTC*/   std::string const &source,
/*AUTO_CTC*/   std::string const &message)
/*AUTO_CTC*/   : m_range(range),
/*AUTO_CTC*/     m_severity(severity),
/*AUTO_CTC*/     m_source(source),
/*AUTO_CTC*/     m_message(message)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Diagnostic::LSP_Diagnostic(LSP_Diagnostic const &obj) noexcept
/*AUTO_CTC*/   : DMEMB(m_range),
/*AUTO_CTC*/     DMEMB(m_severity),
/*AUTO_CTC*/     DMEMB(m_source),
/*AUTO_CTC*/     DMEMB(m_message)
/*AUTO_CTC*/ {}
/*AUTO_CTC*/
/*AUTO_CTC*/ LSP_Diagnostic &LSP_Diagnostic::operator=(LSP_Diagnostic const &obj) noexcept
/*AUTO_CTC*/ {
/*AUTO_CTC*/   if (this != &obj) {
/*AUTO_CTC*/     CMEMB(m_range);
/*AUTO_CTC*/     CMEMB(m_severity);
/*AUTO_CTC*/     CMEMB(m_source);
/*AUTO_CTC*/     CMEMB(m_message);
/*AUTO_CTC*/   }
/*AUTO_CTC*/   return *this;
/*AUTO_CTC*/ }
/*AUTO_CTC*/


LSP_Diagnostic::LSP_Diagnostic(gdv::GDValue const &m)
  : GDV_READ_MEMBER_SK(m_range),
    GDV_READ_MEMBER_SK(m_severity),
    GDV_READ_MEMBER_SK(m_source),
    GDV_READ_MEMBER_SK(m_message)
{}


// ------------------- LSP_PublishDiagnosticsParams --------------------
// create-tuple-class: definitions for LSP_PublishDiagnosticsParams
/*AUTO_CTC*/ LSP_PublishDiagnosticsParams::LSP_PublishDiagnosticsParams(
/*AUTO_CTC*/   std::string const &uri,
/*AUTO_CTC*/   int version,
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


LSP_PublishDiagnosticsParams::LSP_PublishDiagnosticsParams(gdv::GDValue const &m)
  : GDV_READ_MEMBER_SK(m_uri),
    GDV_READ_MEMBER_SK(m_version),
    GDV_READ_MEMBER_SK(m_diagnostics)
{}


// EOF
