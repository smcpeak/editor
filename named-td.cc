// named-td.cc
// code for named-td.h

#include "named-td.h"                  // this module

#include "lsp-conv.h"                  // convertLSPDiagsToTDD
#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // CMEMB
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // replaceAll
#include "smbase/xassert.h"            // xassertPrecondition

#include <cstdint>                     // std::int64_t
#include <utility>                     // std::move

using namespace gdv;


INIT_TRACE("named-td");


int NamedTextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(NamedTextDocument);


NamedTextDocument::NamedTextDocument()
  : TextDocument(),
    m_documentName(),
    m_diagnostics(),
    m_receivedStaleDiagnostics(false),
    m_lastFileTimestamp(0),
    m_modifiedOnDisk(false),
    m_title(),
    m_highlighter(),
    m_highlightTrailingWhitespace(true)
{
  selfCheck();

  NamedTextDocument::s_objectCount++;
}


NamedTextDocument::~NamedTextDocument()
{
  // Explicitly do this so the diagnostics detach while we are still in
  // the explicit part of the dtor, mostly for ease of debugging.
  m_diagnostics.reset(nullptr);

  NamedTextDocument::s_objectCount--;
}


void NamedTextDocument::selfCheck() const
{
  TextDocument::selfCheck();

  m_documentName.selfCheck();

  if (m_diagnostics) {
    m_diagnostics->selfCheck();
  }
}


void NamedTextDocument::setDocumentProcessStatus(DocumentProcessStatus status)
{
  this->TextDocument::setDocumentProcessStatus(status);

  if (this->isProcessOutput()) {
    this->m_highlightTrailingWhitespace = false;
  }
}


HostAndResourceName NamedTextDocument::directoryHarn() const
{
  return HostAndResourceName(hostName(), directory());
}


string NamedTextDocument::applyCommandSubstitutions(string const &orig) const
{
  // If the document does not have a file name, we will return a pair of
  // quotes, signifying an empty string in the context of a shell
  // command.
  string fname = "''";

  if (m_documentName.hasFilename()) {
    SMFileUtil sfu;
    fname = sfu.splitPathBase(m_documentName.filename());
  }

  // Very simple, naive implementation.
  return replaceAll(orig, "$f", fname);
}


static char const *documentProcessStatusIndicator(
  NamedTextDocument const *doc)
{
  switch (doc->documentProcessStatus()) {
    default:
      DEV_WARNING("Bad document process status");
      // fallthrough
    case DPS_NONE:           return "";
    case DPS_RUNNING:        return "<running> ";
    case DPS_FINISHED:       return "<finished> ";
  }
}

string NamedTextDocument::nameWithStatusIndicators() const
{
  std::ostringstream sb;
  sb << documentProcessStatusIndicator(this);
  if (!hostName().isLocal()) {
    sb << hostName() << ": ";
  }
  sb << resourceName();
  sb << fileStatusString();
  return sb.str();
}


string NamedTextDocument::fileStatusString() const
{
  std::ostringstream sb;
  if (this->unsavedChanges()) {
    sb << " *";
  }
  if (m_modifiedOnDisk) {
    sb << " [DISKMOD]";
  }
  return sb.str();
}


void NamedTextDocument::replaceFileAndStats(
  std::vector<unsigned char> const &contents,
  std::int64_t fileModificationTime,
  bool readOnly)
{
  TRACE1(
    "replaceFileAndStats: docName=" << documentName() <<
    " contents.size()=" << contents.size() <<
    " modTime=" << fileModificationTime <<
    " readOnly=" << readOnly);

  this->replaceWholeFile(contents);
  this->m_lastFileTimestamp = fileModificationTime;
  this->m_modifiedOnDisk = false;
  this->setReadOnly(readOnly);
}


GDValue NamedTextDocument::getDiagnosticsSummary() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "NTD_DiagSummary"_sym);

  m.mapSetValueAtSym("numDiagnostics", toGDValue(getNumDiagnostics()));
  GDV_WRITE_MEMBER_SYM(m_receivedStaleDiagnostics);

  return m;
}


std::optional<int> NamedTextDocument::getNumDiagnostics() const
{
  if (m_diagnostics) {
    return m_diagnostics->size();
  }
  else {
    return std::nullopt;
  }
}


TextDocumentDiagnostics const * NULLABLE
NamedTextDocument::getDiagnostics() const
{
  return m_diagnostics.get();
}


bool NamedTextDocument::hasOutOfDateDiagnostics() const
{
  return m_diagnostics != nullptr &&
         getVersionNumber() != m_diagnostics->getOriginVersion();
}


bool NamedTextDocument::hasReceivedStaleDiagnostics() const
{
  return m_receivedStaleDiagnostics;
}


void NamedTextDocument::updateDiagnostics(
  std::unique_ptr<TextDocumentDiagnostics> diagnostics)
{
  m_diagnostics = std::move(diagnostics);
  if (!m_diagnostics) {
    m_receivedStaleDiagnostics = false;
  }
  notifyMetadataChange();
}


RCSerf<TDD_Diagnostic const> NamedTextDocument::getDiagnosticAt(
  TextMCoord tc) const
{
  if (m_diagnostics.get()) {
    return m_diagnostics->getDiagnosticAt(tc);
  }
  else {
    return {};
  }
}


void NamedTextDocument::sentLSPFileContents()
{
  notifyMetadataChange();
}


void NamedTextDocument::receivedLSPDiagnostics(
  LSP_PublishDiagnosticsParams const *diags)
{
  xassertPrecondition(diags->m_version.has_value());

  // Convert the version number data type.
  TextDocument::VersionNumber receivedDiagsVersion =
    static_cast<TextDocument::VersionNumber>(*diags->m_version);

  if (receivedDiagsVersion == getVersionNumber()) {
    TRACE1("received updated diagnostics for " << documentName());

    m_receivedStaleDiagnostics = false;

    updateDiagnostics(convertLSPDiagsToTDD(this, diags));
  }

  else {
    // TODO: Adapt them rather than discard them.
    TRACE1(
      "received diagnostics for " << documentName() <<
      ", but doc is now version " << getVersionNumber() <<
      " while diags have version " << receivedDiagsVersion);

    m_receivedStaleDiagnostics = true;

    notifyMetadataChange();
  }
}


// EOF
