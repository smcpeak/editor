// td-obs-recorder.cc
// Code for `td-obs-recorder` module.

#include "td-obs-recorder.h"           // this module

#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/ast-switch.h"         // DEFN_AST_DOWNCASTS
#include "smbase/exc.h"                // GENERIC_CATCH_{BEGIN,END}
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue-unique-ptr.h" // gdv::toGDValue(std::unique_ptr)
#include "smbase/gdvalue-vector.h"     // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/map-util.h"           // smbase::{mapContains, mapInsertMove, mapKeySet}
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/xassert.h"            // xassertPrecondition

#include <map>                         // std::map
#include <memory>                      // std::unique_ptr
#include <optional>                    // std::optional
#include <set>                         // std::set
#include <utility>                     // std::move
#include <vector>                      // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("td-obs-recorder");


// ------------------------ TextDocumentChange -------------------------
TextDocumentChange::~TextDocumentChange()
{}


TextDocumentChange::TextDocumentChange()
{}


DEFN_AST_DOWNCASTS(TextDocumentChange, TDC_InsertLine, K_INSERT_LINE)
DEFN_AST_DOWNCASTS(TextDocumentChange, TDC_DeleteLine, K_DELETE_LINE)
DEFN_AST_DOWNCASTS(TextDocumentChange, TDC_InsertText, K_INSERT_TEXT)
DEFN_AST_DOWNCASTS(TextDocumentChange, TDC_DeleteText, K_DELETE_TEXT)
DEFN_AST_DOWNCASTS(TextDocumentChange, TDC_TotalChange, K_TOTAL_CHANGE)


// -------------------------- TDC_InsertLine ---------------------------
TDC_InsertLine::~TDC_InsertLine()
{}


TDC_InsertLine::TDC_InsertLine(
  int line, std::optional<int> prevLineBytes)
  : IMEMBFP(line),
    IMEMBFP(prevLineBytes)
{}


void TDC_InsertLine::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->insertLines(m_line, 1);
}


TDC_InsertLine::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "InsertLine"_sym);
  GDV_WRITE_MEMBER_SYM(m_line);
  GDV_WRITE_MEMBER_SYM(m_prevLineBytes);
  return m;
}


// -------------------------- TDC_DeleteLine ---------------------------
TDC_DeleteLine::~TDC_DeleteLine()
{}


TDC_DeleteLine::TDC_DeleteLine(
  int line, std::optional<int> prevLineBytes)
  : IMEMBFP(line),
    IMEMBFP(prevLineBytes)
{}


void TDC_DeleteLine::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->deleteLines(m_line, 1);
}


TDC_DeleteLine::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "DeleteLine"_sym);
  GDV_WRITE_MEMBER_SYM(m_line);
  GDV_WRITE_MEMBER_SYM(m_prevLineBytes);
  return m;
}


// -------------------------- TDC_InsertText ---------------------------
TDC_InsertText::~TDC_InsertText()
{}


TDC_InsertText::TDC_InsertText(
  TextMCoord tc, char const *text, int lengthBytes)
  : IMEMBFP(tc),
    m_text(text, lengthBytes)
{}


void TDC_InsertText::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->insertLineBytes(m_tc, m_text.size());
}


TDC_InsertText::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "InsertText"_sym);
  GDV_WRITE_MEMBER_SYM(m_tc);
  GDV_WRITE_MEMBER_SYM(m_text);
  return m;
}


// -------------------------- TDC_DeleteText ---------------------------
TDC_DeleteText::~TDC_DeleteText()
{}


TDC_DeleteText::TDC_DeleteText(
  TextMCoord tc, int lengthBytes)
  : IMEMBFP(tc),
    IMEMBFP(lengthBytes)
{}


void TDC_DeleteText::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->deleteLineBytes(m_tc, m_lengthBytes);
}


TDC_DeleteText::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "DeleteText"_sym);
  GDV_WRITE_MEMBER_SYM(m_tc);
  GDV_WRITE_MEMBER_SYM(m_lengthBytes);
  return m;
}


// -------------------------- TDC_TotalChange --------------------------
TDC_TotalChange::~TDC_TotalChange()
{}


TDC_TotalChange::TDC_TotalChange(int numLines, std::string &&contents)
  : IMEMBFP(numLines),
    IMEMBMFP(contents)
{}


void TDC_TotalChange::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->clearEverything(m_numLines);
}


TDC_TotalChange::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TotalChange"_sym);
  GDV_WRITE_MEMBER_SYM(m_numLines);
  GDV_WRITE_MEMBER_SYM(m_contents);
  return m;
}


// -------------------------- VersionDetails ---------------------------
TextDocumentObservationRecorder::VersionDetails::~VersionDetails()
{}


TextDocumentObservationRecorder::VersionDetails::VersionDetails(
  VersionDetails &&obj)
  : MDMEMB(m_versionNumber),
    MDMEMB(m_numLines),
    MDMEMB(m_hasDiagnostics),
    MDMEMB(m_changeSequence)
{
  selfCheck();
}


TextDocumentObservationRecorder::VersionDetails::VersionDetails(
  VersionNumber versionNumber, int numLines)
  : IMEMBFP(versionNumber),
    IMEMBFP(numLines),
    m_hasDiagnostics(false),
    m_changeSequence()
{
  selfCheck();
}


void TextDocumentObservationRecorder::VersionDetails::selfCheck() const
{
  xassert(m_numLines >= 0);
}


TextDocumentObservationRecorder::VersionDetails::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "VersionDetails"_sym);
  GDV_WRITE_MEMBER_SYM(m_versionNumber);
  GDV_WRITE_MEMBER_SYM(m_numLines);
  GDV_WRITE_MEMBER_SYM(m_hasDiagnostics);
  GDV_WRITE_MEMBER_SYM(m_changeSequence);
  return m;
}


// --------------- TextDocumentChangeSequence ---------------
TextDocumentChangeSequence::~TextDocumentChangeSequence()
{}


TextDocumentChangeSequence::TextDocumentChangeSequence()
  : m_seq()
{}


TextDocumentChangeSequence::TextDocumentChangeSequence(
  TextDocumentChangeSequence &&obj)
  : MDMEMB(m_seq)
{}


std::size_t TextDocumentChangeSequence::size() const
{
  return m_seq.size();
}


TextDocumentChangeSequence::operator gdv::GDValue() const
{
  return toGDValue(m_seq);
}


// ------------------ TextDocumentObservationRecorder ------------------
TextDocumentObservationRecorder::~TextDocumentObservationRecorder()
{
  m_document.removeObserver(this);
}


TextDocumentObservationRecorder::TextDocumentObservationRecorder(
  TextDocumentCore const &document)
  : TextDocumentObserver(),
    IMEMBFP(document)
{
  m_document.addObserver(this);
}


void TextDocumentObservationRecorder::selfCheck() const
{
  bool isFirstElement = true;

  for (auto const &kv : m_versionToDetails) {
    VersionNumber vn = kv.first;
    VersionDetails const &details = kv.second;

    xassert(details.m_versionNumber == vn);
    details.selfCheck();

    if (!isFirstElement) {
      xassert(details.m_hasDiagnostics == false);
    }
    isFirstElement = false;
  }
}


TextDocumentObservationRecorder::operator gdv::GDValue() const
{
  GDValue m(GDVK_MAP);

  for (auto const &kv : m_versionToDetails) {
    VersionNumber version = kv.first;
    VersionDetails const &details = kv.second;

    // Map from version to the associated details.
    m.mapSetValueAt(version, toGDValue(details));
  }

  return m;
}


bool TextDocumentObservationRecorder::trackingSomething() const
{
  return !m_versionToDetails.empty();
}


auto TextDocumentObservationRecorder::getEarliestVersion() const
  -> std::optional<VersionNumber>
{
  if (m_versionToDetails.empty()) {
    return std::nullopt;
  }
  else {
    auto it = m_versionToDetails.begin();
    return (*it).first;
  }
}


bool TextDocumentObservationRecorder::earliestVersionHasDiagnostics() const
{
  if (m_versionToDetails.empty()) {
    return false;
  }
  else {
    auto it = m_versionToDetails.begin();
    return (*it).second.m_hasDiagnostics;
  }
}


bool TextDocumentObservationRecorder::isTracking(
  VersionNumber version) const
{
  return mapContains(m_versionToDetails, version);
}


auto TextDocumentObservationRecorder::getTrackedVersions() const
  -> std::set<VersionNumber>
{
  return mapKeySet(m_versionToDetails);
}


auto TextDocumentObservationRecorder::getNoDiagsVersions() const
  -> std::set<VersionNumber>
{
  std::set<VersionNumber> ret = getTrackedVersions();
  if (earliestVersionHasDiagnostics()) {
    ret.erase(ret.begin());
  }
  return ret;
}


void TextDocumentObservationRecorder::beginTrackingCurrentDoc()
{
  VersionNumber version = m_document.getVersionNumber();
  int numLines = m_document.numLines();

  if (!mapInsertMove(m_versionToDetails,
                     version,
                     VersionDetails(version, numLines))) {
    // This isn't a problem, but it is noteworthy.
    TRACE1("beginTracking: we are already waiting for version " <<
           version);
  }
}


void TextDocumentObservationRecorder::applyChangesToDiagnostics(
  TextDocumentDiagnostics *diagnostics)
{
  // The document version from which the diagnostics were generated.
  VersionNumber diagVersion = diagnostics->getOriginVersion();

  xassertPrecondition(isTracking(diagVersion));

  // Process the tracked versions in order.
  while (!m_versionToDetails.empty()) {
    auto it = m_versionToDetails.begin();
    VersionNumber trackedVersion = (*it).first;
    VersionDetails &details = (*it).second;

    if (trackedVersion < diagVersion) {
      // This version is older than what we care about; discard it.
      TRACE1("Discarding unneeded old version: " << trackedVersion);
      m_versionToDetails.erase(it);
    }

    else {
      // The sequence now begins at the right spot.
      xassert(trackedVersion == diagVersion);

      // The received diagnostics need to know the number of lines in
      // the file in order to process updates (which is what we're about
      // to do), and they need first to be confined to that number of
      // lines in case they have bogus data.
      TRACE1("Setting num lines to: " << details.m_numLines);
      diagnostics->setNumLinesAndAdjustAccordingly(details.m_numLines);

      // These details are now the one that has diagnostics.
      details.m_hasDiagnostics = true;

      break;
    }
  }

  // Now, walk the map, applying all recorded changes in ascending
  // version order.  This brings `diagnostics` up to date with all
  // changes that have been made to the document.
  for (auto const &kv : m_versionToDetails) {
    VersionNumber trackedVersion = kv.first;
    VersionDetails const &details = kv.second;

    TRACE1("Rolling forward from version " << trackedVersion <<
           " by applying " << details.m_changeSequence.size() <<
           " observed changes.");

    // Walk the sequence, applying changes in order.
    for (std::unique_ptr<TextDocumentChange> const &obsPtr :
           details.m_changeSequence.m_seq) {
      obsPtr->applyChangeToDiagnostics(diagnostics);
    }
  }
}


auto TextDocumentObservationRecorder::getLastTrackedVersionC() const
  -> VersionDetails const &
{
  xassertPrecondition(trackingSomething());

  // Get last entry.
  auto it = m_versionToDetails.rbegin();
  xassert(it != m_versionToDetails.rend());
  return (*it).second;
}


auto TextDocumentObservationRecorder::getLastTrackedVersion()
  -> VersionDetails &
{
  return const_cast<VersionDetails &>(getLastTrackedVersionC());
}


TextDocumentChangeSequence const &
TextDocumentObservationRecorder::getUnsentChanges() const
{
  return getLastTrackedVersionC().m_changeSequence;
}


void TextDocumentObservationRecorder::addObservation(
  std::unique_ptr<TextDocumentChange> observation)
{
  // Append the record to the last version.
  getLastTrackedVersion().
    m_changeSequence.m_seq.push_back(std::move(observation));
}


void TextDocumentObservationRecorder::observeInsertLine(
  TextDocumentCore const &doc, int line) noexcept
{
  GENERIC_CATCH_BEGIN

  if (trackingSomething()) {
    std::optional<int> prevLineBytes;
    if (line == doc.numLines() - 1) {
      // We just inserted a new last line.  (`doc` already has the
      // change applied to it).
      prevLineBytes = doc.lineLengthBytes(line-1);
    }

    addObservation(
      std::make_unique<TDC_InsertLine>(line, prevLineBytes));
  }

  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeDeleteLine(
  TextDocumentCore const &doc, int line) noexcept
{
  GENERIC_CATCH_BEGIN

  if (trackingSomething()) {
    std::optional<int> prevLineBytes;
    if (line == doc.numLines()) {
      // We just deleted the last line.
      prevLineBytes = doc.lineLengthBytes(line-1);
    }

    addObservation(
      std::make_unique<TDC_DeleteLine>(line, prevLineBytes));
  }

  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeInsertText(
  TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDC_InsertText>(tc, text, lengthBytes));
  }
  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeDeleteText(
  TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDC_DeleteText>(tc, lengthBytes));
  }
  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeTotalChange(
  TextDocumentCore const &doc) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDC_TotalChange>(
      doc.numLines(),
      doc.getWholeFileString()));
  }
  GENERIC_CATCH_END
}


// EOF
