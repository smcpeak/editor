// named-td.cc
// code for named-td.h

#include "named-td.h"                  // this module

#include "lsp-conv.h"                  // convertLSPDiagsToTDD
#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "td-diagnostics.h"            // TextDocumentDiagnostics
#include "td-obs-recorder.h"           // TextDocumentObservationRecorder

#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-unique-ptr.h" // gdv::toGDValue(std::unique_ptr)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/map-util.h"           // smbase::mapContains
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/overflow.h"           // smbase::convertNumber
#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // CMEMB
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // replaceAll
#include "smbase/xassert.h"            // xassertPrecondition

#include <cstdint>                     // std::int64_t
#include <utility>                     // std::move

using namespace gdv;
using namespace smbase;


INIT_TRACE("named-td");


int NamedTextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(NamedTextDocument);


NamedTextDocument::NamedTextDocument()
  : TextDocument(),
    m_documentName(),
    m_diagnostics(),
    m_tddUpdater(),
    m_observationRecorder(getCore()),
    m_lastFileTimestamp(0),
    m_modifiedOnDisk(false),
    m_title(),
    m_highlighter(),
    m_highlightTrailingWhitespace(true),
    m_lspUpdateContinuously(true)
{
  selfCheck();

  NamedTextDocument::s_objectCount++;
}


NamedTextDocument::~NamedTextDocument()
{
  // Explicitly do this so the diagnostics detach while we are still in
  // the explicit part of the dtor, mostly for ease of debugging.
  m_tddUpdater.reset(nullptr);
  m_diagnostics.reset(nullptr);

  NamedTextDocument::s_objectCount--;
}


void NamedTextDocument::selfCheck() const
{
  TextDocument::selfCheck();

  m_documentName.selfCheck();

  xassert((m_diagnostics==nullptr) == (m_tddUpdater==nullptr));

  if (m_diagnostics) {
    m_diagnostics->selfCheck();

    xassert(m_tddUpdater->getDiagnostics() == m_diagnostics.get());
    xassert(m_tddUpdater->getDocument() == this);

    m_tddUpdater->selfCheck();

    // We should not be waiting for any version for which we already
    // have more recent diagnostics.
    if (std::optional<VersionNumber> earliestTracked =
          m_observationRecorder.getEarliestVersion()) {
      xassert(*earliestTracked >= m_diagnostics->getOriginVersion());
    }
  }

  m_observationRecorder.selfCheck();
}


NamedTextDocument::operator gdv::GDValue() const
{
  GDValue m(TextDocument::operator GDValue());
  m.taggedContainerSetTag("NamedTextDocument"_sym);

  GDV_WRITE_MEMBER_SYM(m_documentName);
  GDV_WRITE_MEMBER_SYM(m_diagnostics);

  // m_tddUpdater simply contains two serf pointers, so does not have
  // anything useful to contribute to a `GDValue`.  But we can record
  // whether it is present.
  m.mapSetValueAtSym("hasTddUpdater", m_tddUpdater.operator bool());

  GDV_WRITE_MEMBER_SYM(m_observationRecorder);
  GDV_WRITE_MEMBER_SYM(m_lastFileTimestamp);
  GDV_WRITE_MEMBER_SYM(m_modifiedOnDisk);
  GDV_WRITE_MEMBER_SYM(m_title);
  GDV_WRITE_MEMBER_SYM(m_highlighter);
  GDV_WRITE_MEMBER_SYM(m_highlightTrailingWhitespace);
  GDV_WRITE_MEMBER_SYM(m_lspUpdateContinuously);

  return m;
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


bool NamedTextDocument::isCompatibleWithLSP() const
{
  return !isIncompatibleWithLSP().has_value();
}


std::optional<std::string>
NamedTextDocument::isIncompatibleWithLSP() const
{
  if (m_documentName.isLocalFilename()) {
    return std::nullopt;
  }
  else {
    return "LSP only works with local files.";
  }
}


GDValue NamedTextDocument::getDiagnosticsSummary() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "NTD_DiagSummary"_sym);

  m.mapSetValueAtSym("numDiagnostics", toGDValue(getNumDiagnostics()));

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


void NamedTextDocument::updateDiagnostics(
  std::unique_ptr<TextDocumentDiagnostics> diagnostics)
{
  if (diagnostics) {
    // The document version from which the diagnostics were generated.
    VersionNumber diagVersion = diagnostics->getOriginVersion();
    TRACE1("updateDiagnostics: Received diagnostics for version " <<
           diagVersion << " of " << documentName());

    if (!m_observationRecorder.isTracking(diagVersion)) {
      // We cannot roll the diagnostics forward, so we cannot use them.
      // Do not change anything.
      TRACE1("updateDiagnostics: Received diagnostics I wasn't "
             "expecting.  Discarding them.");
      return;
    }

    // Roll the diagnostics forward to account for changes made to the
    // document since `diagVersion`.  Additionally, discard the change
    // records associated with all versions before `diagVersion`.
    m_observationRecorder.applyChangesToDiagnostics(diagnostics.get());

    // Modify `m_diagnostics` so it conforms to this document's shape.
    // This is necessary even with version roll-forward because the
    // diagnostic source could have generated arbitrary junk (e.g.,
    // byte indices within a line that are too large).
    diagnostics->adjustForDocument(getCore());

    // Double-check them before adding.
    diagnostics->selfCheck();

    // We are about to deallocate the existing diagnostics, so detach
    // the updater.
    m_tddUpdater.reset(nullptr);

    // Replace the existing diagnostics, if any.
    m_diagnostics = std::move(diagnostics);

    // Create the object that allows `m_diagnostics` to track the
    // subsequent file modifications made by the user.
    m_tddUpdater.reset(
      new TextDocumentDiagnosticsUpdater(m_diagnostics.get(), this));
  }

  else {
    // Reset all diagnostics state.
    m_tddUpdater.reset(nullptr);
    m_diagnostics.reset(nullptr);
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


void NamedTextDocument::beginTrackingChanges()
{
  TRACE1("beginTrackingChanges: version is " << getVersionNumber());

  m_observationRecorder.beginTrackingCurrentDoc();

  // Alert observers that a request for diagnostics is in flight.
  notifyMetadataChange();
}


bool NamedTextDocument::trackingChanges() const
{
  return m_observationRecorder.trackingSomething();
}


RCSerf<TextDocumentChangeSequence const>
NamedTextDocument::getUnsentChanges() const
{
  xassert(trackingChanges());
  return m_observationRecorder.getUnsentChanges();
}


void NamedTextDocument::stopTrackingChanges()
{
  m_observationRecorder.clear();
}


// EOF
