// td-obs-recorder.h
// `TextDocumentObservationRecorder` class.

// See license.txt for copyright and terms of use.

// See design rationale in `doc/td-obs-recorder-design.txt`.

#ifndef EDITOR_TD_OBS_RECORDER_H
#define EDITOR_TD_OBS_RECORDER_H

#include "td-obs-recorder-fwd.h"       // fwds for this module

#include "td-core.h"                   // TextDocumentObserver
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/std-optional-fwd.h"   // std::optional
#include "smbase/std-set-fwd.h"        // stdfwd::set

#include <map>                         // std::map
#include <memory>                      // std::unique_ptr
#include <optional>                    // std::optional
#include <vector>                      // std::vector


// A record of one of the changes that can be observed via the
// `TextDocumentObserver` interface.
class TextDocumentChangeObservation {
public:      // types
  // Enumeration of all concrete `TextDocumentChangeObservation`
  // subclasses.
  enum ObsKind {
    OK_INSERT_LINE,
    OK_DELETE_LINE,
    OK_INSERT_TEXT,
    OK_DELETE_TEXT,
    OK_TOTAL_CHANGE
  };

public:      // methods
  virtual ~TextDocumentChangeObservation();

  explicit TextDocumentChangeObservation();

  // Specific subclass.
  virtual ObsKind kind() const = 0;

  // Apply the change recored in this object to `diagnostics`.
  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const = 0;

  // Dump data for testing and debugging.
  virtual operator gdv::GDValue() const = 0;
};


// Records `observeInsertLine`.
class TDCO_InsertLine : public TextDocumentChangeObservation {
public:      // data
  // Observer method arguments.
  int m_line;

  // If set, then `m_line` is to become the new last line in the
  // document.  In order to express this as a range replacement, we need
  // to know the length of the previous line in bytes.
  std::optional<int> m_prevLineBytes;

public:      // methods
  virtual ~TDCO_InsertLine() override;

  explicit TDCO_InsertLine(int line, std::optional<int> prevLineBytes);

  virtual ObsKind kind() const override
    { return OK_INSERT_LINE; }

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeDeleteLine`.
class TDCO_DeleteLine : public TextDocumentChangeObservation {
public:      // data
  // Observer method arguments.
  int m_line;

  // If set, then `m_line` is the last line in the document.  In order
  // to express this deletion as a range replacement, we need to know
  // the length of the previous line in bytes.
  std::optional<int> m_prevLineBytes;

public:      // methods
  virtual ~TDCO_DeleteLine() override;

  explicit TDCO_DeleteLine(int line, std::optional<int> prevLineBytes);

  virtual ObsKind kind() const override
    { return OK_DELETE_LINE; }

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeInsertText`.
class TDCO_InsertText : public TextDocumentChangeObservation {
public:      // data
  // Observer method arguments.
  TextMCoord m_tc;

  // Although not needed for replaying to diagnostics, this is needed
  // for incremental content update for LSP.
  //
  // The length of `m_text` is the original `lengthBytes` argument.
  std::string m_text;

public:      // methods
  virtual ~TDCO_InsertText() override;

  explicit TDCO_InsertText(
    TextMCoord tc, char const *text, int lengthBytes);

  virtual ObsKind kind() const override
    { return OK_INSERT_TEXT; }

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeDeleteText`.
class TDCO_DeleteText : public TextDocumentChangeObservation {
public:      // data
  // Observer method arguments.
  TextMCoord m_tc;
  int m_lengthBytes;

public:      // methods
  virtual ~TDCO_DeleteText() override;

  explicit TDCO_DeleteText(
    TextMCoord tc, int lengthBytes);

  virtual ObsKind kind() const override
    { return OK_DELETE_TEXT; }

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeTotalChange`.
class TDCO_TotalChange : public TextDocumentChangeObservation {
public:      // data
  // Number of lines in the document after the change.
  int m_numLines;

  // Full contents.
  std::string m_contents;

public:      // methods
  virtual ~TDCO_TotalChange() override;

  explicit TDCO_TotalChange(int m_numLines, std::string &&contents);

  virtual ObsKind kind() const override
    { return OK_TOTAL_CHANGE; }

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Sequence of changes.
class TextDocumentChangeObservationSequence {
  NO_OBJECT_COPIES(TextDocumentChangeObservationSequence);

public:      // data
  // A sequence of changes that were applied to the document in the
  // order they happened.
  std::vector<std::unique_ptr<TextDocumentChangeObservation>>
    m_seq;

public:
  ~TextDocumentChangeObservationSequence();

  // Initially empty sequence.
  TextDocumentChangeObservationSequence();

  TextDocumentChangeObservationSequence(TextDocumentChangeObservationSequence &&obj);

  std::size_t size() const;

  operator gdv::GDValue() const;
};


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
    TextDocumentChangeObservationSequence m_changeSequence;

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
    stdfwd::unique_ptr<TextDocumentChangeObservation> observation);

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
  std::optional<VersionNumber> getEarliestVersion() const;

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
  TextDocumentChangeObservationSequence const &getUnsentChanges() const;

  // TextDocumentObserver methods.
  virtual void observeInsertLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeDeleteLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept override;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept override;
  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept override;
};


#endif // EDITOR_TD_OBS_RECORDER_H
