// td-obs-recorder.h
// `TextDocumentObservationRecorder` class.

// See license.txt for copyright and terms of use.

// See design rationale in `doc/td-obs-recorder-design.txt`.

#ifndef EDITOR_TD_OBS_RECORDER_H
#define EDITOR_TD_OBS_RECORDER_H

#include "td-obs-recorder-fwd.h"       // fwds for this module

#include "td-change-fwd.h"             // TextDocumentChange [n]
#include "td-change-seq.h"             // TextDocumentChangeSequence
#include "td-core.h"                   // TextDocumentObserver
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics [n]

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr [n]
#include "smbase/std-optional-fwd.h"   // std::optional [n]
#include "smbase/std-set-fwd.h"        // stdfwd::set [n]

#include <map>                         // std::map


/* Records the sequence of changes observed via the
   `TextDocumentObserver` interface, associated with the document
   versions to which they apply.

   Conceptually, we can think of the history of a document as a
   sequence of versions separated by changes:

     version 1               most recent with diagnostics
        |
        | changes 1 -> 2
        V
     version 2               awaiting diagnostics
        |
        | changes 2 -> 3
        V
     version 3               awaiting diagnostics
        |
        | changes 3 -> current
        V
     (current version)

   This class records a suffix of the document's complete history,
   organized as a map from a version number to the sequence of changes
   that were applied to get to the *next* version.  The versions that
   are keys in the map are called the "tracked" versions.  There might
   not be any tracked versions.

   The set of tracked versions is (1) any for which we have sent the
   contents to the LSP server but not yet received a diagnostics reply,
   and (2) the version for which we most recently received diagnostics.
   If a version has diagnostics, then it is the first (oldest); any
   older that might still (somehow) be awaiting diagnostics are
   discarded when later diagnostics are received.

   Among the operations this supports is sending all of the changes
   associated with the latest tracked version to the server in order to
   initiate the process of bringing it up to date.  In the diagram, the
   "changes 3 -> current" is that set of changes.

   In the quiescent fully up-to-date state, there is a single tracked
   version, and we have diagnostics for it, and there are no recorded
   changes associated with it.
*/
class TextDocumentObservationRecorder : public TextDocumentObserver {
public:      // types
  typedef TextDocumentCore::VersionNumber VersionNumber;

private:     // types
  // Data associated with a document version.
  class VersionDetails {
    NO_OBJECT_COPIES(VersionDetails);

  public:      // data
    // The version number this object describes.
    VersionNumber const m_versionNumber;

    // Number of lines that were in the file for this version.  It is
    // non-negative.
    int const m_numLines;

    // True if this is a version for which we have received diagnostics.
    // It is initially false.
    //
    // The diagnostics are not actually stored here, but instead in a
    // `TextDocumentDiagnostics` object (`td-diagnostics.h`), itself
    // contained in a `NamedTextDocument` (`named-td.h`) that also
    // contains the `TextDocumentObservationRecorder`.
    //
    bool m_hasDiagnostics;

    // Changes that were applied to this document since
    // `m_versionNumber` was current, but before a later version started
    // being tracked.
    TextDocumentChangeSequence m_changeSequence;

  public:
    ~VersionDetails();

    VersionDetails(VersionDetails &&obj);

    // The sequence is initially empty.
    explicit VersionDetails(VersionNumber versionNumber, int numLines);

    void selfCheck() const;

    operator gdv::GDValue() const;
  };

private:     // data
  // The document we are observing.
  TextDocumentCore const &m_document;

  // Map from document version number to its tracked details.
  //
  // Invariant: for all `vn`:
  //   m_versionToDetails[vn].m_versionNumber == vn
  //
  // Invariant: for all `vn` except the first:
  //   m_versionToDetails[vn].m_hasDiagnostics == false
  //
  std::map<VersionNumber, VersionDetails> m_versionToDetails;

private:     // methods
  // Get the most recent tracked version.
  //
  // Requires: trackingSomething()
  VersionDetails const &getLastTrackedVersionC() const;
  VersionDetails &getLastTrackedVersion();

  // Append `observation` to the latest tracked version.
  //
  // Requires: trackingSomething()
  void addObservation(
    stdfwd::unique_ptr<TextDocumentChange> observation);

public:      // methods
  virtual ~TextDocumentObservationRecorder() override;

  explicit TextDocumentObservationRecorder(
    TextDocumentCore const &document);

  // Assert invariants.
  void selfCheck() const;

  // Dump for test/debug.
  operator gdv::GDValue() const;

  // True if we are tracking at least one version.
  bool trackingSomething() const;

  // Return the earliest version for which we have the ability to roll
  // changes forward.
  stdfwd::optional<VersionNumber> getEarliestVersion() const;

  // True if we are tracking at least one version, and that version has
  // received diagnostics.
  bool earliestVersionHasDiagnostics() const;

  // True if we are tracking `version` and hence can roll forward
  // changes from there.
  bool isTracking(VersionNumber version) const;

  // Get the set of all versions being tracked.
  stdfwd::set<VersionNumber> getTrackedVersions() const;

  // Get the set of all versions being tracked that do not have
  // diagnostics.
  stdfwd::set<VersionNumber> getNoDiagsVersions() const;

  // Track all future changes as applying on top of the current version
  // of `m_document`.
  void beginTrackingCurrentDoc();

  // Apply the changes we recorded to `diagnostics`.  Discard the
  // information for its version and all earlier ones.
  //
  // Before applying changes, this call enables change tracking for
  // `diagnostics` by supplying it with the number of lines that was
  // supplied to `beginTracking`.
  //
  // Requires: isTracking(diagnostics->getOriginVersion())
  void applyChangesToDiagnostics(TextDocumentDiagnostics *diagnostics);

  // Return the sequence of changes that have been observed but not yet
  // sent to the server.
  //
  // Requires: trackingSomething()
  TextDocumentChangeSequence const &getUnsentChanges() const;

  // TextDocumentObserver methods.
  virtual void observeInsertLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeDeleteLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept override;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept override;
  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept override;
};


#endif // EDITOR_TD_OBS_RECORDER_H
