// td-obs-recorder.cc
// Code for `td-obs-recorder` module.

#include "td-obs-recorder.h"           // this module

#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/exc.h"                // GENERIC_CATCH_{BEGIN,END}
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


// ------------------- TextDocumentChangeObservation -------------------
TextDocumentChangeObservation::~TextDocumentChangeObservation()
{}


TextDocumentChangeObservation::TextDocumentChangeObservation()
{}


// -------------------------- TDCO_InsertLine --------------------------
TDCO_InsertLine::~TDCO_InsertLine()
{}


TDCO_InsertLine::TDCO_InsertLine(int line)
  : IMEMBFP(line)
{}


void TDCO_InsertLine::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->insertLines(m_line, 1);
}


TDCO_InsertLine::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "InsertLine"_sym);
  GDV_WRITE_MEMBER_SYM(m_line);
  return m;
}


// -------------------------- TDCO_DeleteLine --------------------------
TDCO_DeleteLine::~TDCO_DeleteLine()
{}


TDCO_DeleteLine::TDCO_DeleteLine(int line)
  : IMEMBFP(line)
{}


void TDCO_DeleteLine::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->deleteLines(m_line, 1);
}


TDCO_DeleteLine::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "DeleteLine"_sym);
  GDV_WRITE_MEMBER_SYM(m_line);
  return m;
}


// -------------------------- TDCO_InsertText --------------------------
TDCO_InsertText::~TDCO_InsertText()
{}


TDCO_InsertText::TDCO_InsertText(
  TextMCoord tc, char const *text, int lengthBytes)
  : IMEMBFP(tc),
    m_text(text, lengthBytes)
{}


void TDCO_InsertText::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->insertLineBytes(m_tc, m_text.size());
}


TDCO_InsertText::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "InsertText"_sym);
  GDV_WRITE_MEMBER_SYM(m_tc);
  GDV_WRITE_MEMBER_SYM(m_text);
  return m;
}


// -------------------------- TDCO_DeleteText --------------------------
TDCO_DeleteText::~TDCO_DeleteText()
{}


TDCO_DeleteText::TDCO_DeleteText(
  TextMCoord tc, int lengthBytes)
  : IMEMBFP(tc),
    IMEMBFP(lengthBytes)
{}


void TDCO_DeleteText::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->deleteLineBytes(m_tc, m_lengthBytes);
}


TDCO_DeleteText::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "DeleteText"_sym);
  GDV_WRITE_MEMBER_SYM(m_tc);
  GDV_WRITE_MEMBER_SYM(m_lengthBytes);
  return m;
}


// ------------------------- TDCO_TotalChange --------------------------
TDCO_TotalChange::~TDCO_TotalChange()
{}


TDCO_TotalChange::TDCO_TotalChange(int numLines)
  : IMEMBFP(numLines)
{}


void TDCO_TotalChange::applyChangeToDiagnostics(
  TextDocumentDiagnostics *diagnostics) const
{
  diagnostics->clearEverything(m_numLines);
}


TDCO_TotalChange::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TotalChange"_sym);
  GDV_WRITE_MEMBER_SYM(m_numLines);
  return m;
}


// -------------------------- VersionDetails ---------------------------
TextDocumentObservationRecorder::VersionDetails::~VersionDetails()
{}


TextDocumentObservationRecorder::VersionDetails::VersionDetails(
  VersionDetails &&obj)
  : MDMEMB(m_versionNumber),
    MDMEMB(m_numLines),
    MDMEMB(m_changeSequence)
{
  selfCheck();
}


TextDocumentObservationRecorder::VersionDetails::VersionDetails(
  VersionNumber versionNumber, int numLines)
  : IMEMBFP(versionNumber),
    IMEMBFP(numLines),
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
  GDV_WRITE_MEMBER_SYM(m_changeSequence);
  return m;
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
  for (auto const &kv : m_versionToDetails) {
    VersionNumber vn = kv.first;
    VersionDetails const &details = kv.second;

    xassert(details.m_versionNumber == vn);
    details.selfCheck();
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


void TextDocumentObservationRecorder::beginTracking(
  VersionNumber version, int numLines)
{
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
    VersionDetails const &details = (*it).second;

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
    for (std::unique_ptr<TextDocumentChangeObservation> const &obsPtr :
           details.m_changeSequence) {
      obsPtr->applyChangeToDiagnostics(diagnostics);
    }
  }

  // Discard the first element since we have no more need to replay
  // changes from `diagVersion`, as the updated `diagnostics` will
  // become the current diagnostics, and its origin version is at least
  // as recent, so if new diagnostics arrive for that same version,
  // we'll just discard them.
  auto it = m_versionToDetails.begin();
  xassert(it != m_versionToDetails.end());
  xassert((*it).first == diagVersion);
  m_versionToDetails.erase(diagVersion);

  // Note: We do not discard all recorded changes since there could be
  // another set of diagnostics, derived from a more recent version,
  // still in flight right now.
}


void TextDocumentObservationRecorder::addObservation(
  std::unique_ptr<TextDocumentChangeObservation> observation)
{
  xassertPrecondition(trackingSomething());

  // Get the *last* version we are tracking.
  auto it = m_versionToDetails.rbegin();
  xassert(it != m_versionToDetails.rend());

  // Append the record to that version.
  (*it).second.m_changeSequence.push_back(std::move(observation));
}


void TextDocumentObservationRecorder::observeInsertLine(
  TextDocumentCore const &doc, int line) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDCO_InsertLine>(line));
  }
  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeDeleteLine(
  TextDocumentCore const &doc, int line) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDCO_DeleteLine>(line));
  }
  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeInsertText(
  TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDCO_InsertText>(tc, text, lengthBytes));
  }
  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeDeleteText(
  TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDCO_DeleteText>(tc, lengthBytes));
  }
  GENERIC_CATCH_END
}


void TextDocumentObservationRecorder::observeTotalChange(
  TextDocumentCore const &doc) noexcept
{
  GENERIC_CATCH_BEGIN
  if (trackingSomething()) {
    addObservation(std::make_unique<TDCO_TotalChange>(doc.numLines()));
  }
  GENERIC_CATCH_END
}


// EOF
