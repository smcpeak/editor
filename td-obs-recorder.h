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
#include <vector>                      // std::vector


// A record of one of the changes that can be observed via the
// `TextDocumentObserver` interface.
class TextDocumentChangeObservation {
public:      // methods
  virtual ~TextDocumentChangeObservation();

  explicit TextDocumentChangeObservation();

  // Apply the change recored in this object to `diagnostics`.
  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const = 0;

  // Dump data for testing and debugging.
  virtual operator gdv::GDValue() const = 0;
};


// Records `observeInsertLine`.
class TDCO_InsertLine : public TextDocumentChangeObservation {
private:     // data
  // Observer method arguments.
  int m_line;

public:      // methods
  virtual ~TDCO_InsertLine() override;

  explicit TDCO_InsertLine(int line);

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeDeleteLine`.
class TDCO_DeleteLine : public TextDocumentChangeObservation {
private:     // data
  // Observer method arguments.
  int m_line;

public:      // methods
  virtual ~TDCO_DeleteLine() override;

  explicit TDCO_DeleteLine(int line);

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeInsertText`.
class TDCO_InsertText : public TextDocumentChangeObservation {
private:     // data
  // Observer method arguments.
  TextMCoord m_tc;

  // Although not needed for replaying to diagnostics, I'll keep a copy
  // of the text in case I want to use this for something else.  The
  // length of `m_text` is the original `lengthBytes` argument.
  std::string m_text;

public:      // methods
  virtual ~TDCO_InsertText() override;

  explicit TDCO_InsertText(
    TextMCoord tc, char const *text, int lengthBytes);

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeDeleteText`.
class TDCO_DeleteText : public TextDocumentChangeObservation {
private:     // data
  // Observer method arguments.
  TextMCoord m_tc;
  int m_lengthBytes;

public:      // methods
  virtual ~TDCO_DeleteText() override;

  explicit TDCO_DeleteText(
    TextMCoord tc, int lengthBytes);

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeTotalChange`.
class TDCO_TotalChange : public TextDocumentChangeObservation {
private:     // data
  // Number of lines in the document after the change.
  int m_numLines;

public:      // methods
  virtual ~TDCO_TotalChange() override;

  explicit TDCO_TotalChange(int m_numLines);

  virtual void applyChangeToDiagnostics(
    TextDocumentDiagnostics *diagnostics) const override;
  virtual operator gdv::GDValue() const override;
};


// Records the sequence of changes observed via the
// `TextDocumentObserver` interface, associated with the document
// versions to which they apply.
class TextDocumentObservationRecorder : public TextDocumentObserver {
private:     // types
  typedef TextDocumentCore::VersionNumber VersionNumber;

  // A sequence of changes that were applied to the document since some
  // point in the past, in the order they happened.  The relevant "point
  // in the past" depends on where this sequence is stored.
  typedef std::vector<std::unique_ptr<TextDocumentChangeObservation>>
    ChangeSequence;

  // Data associated with a document version for which we are awaiting
  // the associated diagnostics.
  class AwaitingDiagnostics {
    // Not implemented, although it could be if needed.
    void operator=(AwaitingDiagnostics const &) = delete;

  public:      // data
    // Number of lines that were in the file for this version.  It is
    // non-negative.
    int const m_numLines;

    // Changes that were applied to this document, since the version
    // with which this object is associated was current, but before a
    // later version started awaiting diagnostics.
    ChangeSequence m_changeSequence;

  public:
    ~AwaitingDiagnostics();

    AwaitingDiagnostics(AwaitingDiagnostics &&obj);

    // The sequence is initially empty.
    explicit AwaitingDiagnostics(int numLines);

    void selfCheck() const;

    operator gdv::GDValue() const;
  };

private:     // data
  // The document we are observing.
  TextDocumentCore const &m_document;

  // Map from document version number to its tracked details.
  std::map<VersionNumber, AwaitingDiagnostics> m_awaitingDiagnostics;

private:     // methods
  // Append `observation` to the latest tracked version.
  //
  // Requires `trackingSomething()`.
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

  // True if we are tracking `version` and hence can roll forward
  // changes from there.
  bool isTracking(VersionNumber version) const;

  // Get the set of all versions being tracked.
  stdfwd::set<VersionNumber> getTrackedVersions() const;

  // Track all future changes as applying on top of `version`, which has
  // `numLines` lines.
  void beginTracking(VersionNumber version, int numLines);

  // Apply the changes we recorded to `diagnostics`.  Discard the
  // information for its version and all earlier ones.
  //
  // Before applying changes, this call enables change tracking for
  // `diagnostics` by supplying it with the number of lines that was
  // supplied to `beginTracking`.
  //
  // Requires: isTracking(diagnostics->getOriginVersion())
  void applyChangesToDiagnostics(TextDocumentDiagnostics *diagnostics);

  // TextDocumentObserver methods.
  virtual void observeInsertLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeDeleteLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept override;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept override;
  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept override;
};


#endif // EDITOR_TD_OBS_RECORDER_H
