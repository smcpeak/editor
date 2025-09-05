// td-diagnostics.h
// Classes for representing diagnostics associated with a text document.

// See license.txt for copyright and terms of use.

// This module represents diagnostics in a way that is independent of
// any particular source or communication mechanism, and is as natural
// as possible for the editor program to work with.  In contrast, the
// `lsp-data` module represents diagnostics (logically) as they come
// over the wire from the LSP server, and the `lsp-conv` module
// translates one into the other.

#ifndef EDITOR_TD_DIAGNOSTICS_H
#define EDITOR_TD_DIAGNOSTICS_H

#include "td-diagnostics-fwd.h"        // fwds for this module

#include "byte-count.h"                // ByteCount
#include "byte-index.h"                // ByteIndex
#include "line-count.h"                // LineCount, PositiveLineCount
#include "line-index.h"                // LineIndex
#include "line-number.h"               // LineNumber
#include "named-td-fwd.h"              // NamedTextDocument [n]
#include "td-change-fwd.h"             // TextDocumentChange [n]
#include "td-change-seq-fwd.h"         // TextDocumentChangeSequence [n]
#include "td-core.h"                   // TextDocumentObserver, TextDocumentCore
#include "textmcoord-map.h"            // TextMCoordMap

#include "smbase/compare-util-iface.h" // DEFINE_FRIEND_RELATIONAL_OPERATORS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/refct-serf.h"         // RCSerf, SerfRefCount

#include <string>                      // std::string
#include <optional>                    // std::optional
#include <vector>                      // std::related


// Some information associated with a location, related to a primary
// diagnostic.
class TDD_Related {
public:      // data
  // Absolute file name.
  std::string m_file;

  // 1-based line number.
  //
  // TODO: Change to `LineIndex`.
  //
  // Unlike the primary, the related message locations do not get
  // updated automatically when the file is edited.
  //
  // TODO: Maybe design a way that they can be?
  LineNumber m_line;

  // Relevance of this line to the primary diagnostic.
  std::string m_message;

public:      // methods
  ~TDD_Related();
  TDD_Related(std::string &&file, LineNumber line, std::string &&message);

  operator gdv::GDValue() const;

  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TDD_Related)
};


// A single diagnostic message.
//
// Aside from the locations in `m_related`, this object is not directly
// associated with a location.  Instead, the `TextDocumentDiagnostics`
// map keeps the association with a range, and has facilities for
// updating the association as the document changes.
//
class TDD_Diagnostic : public SerfRefCount {
public:      // data
  // What the diagnostic says.
  std::string m_message;

  // Sequence of related locations.
  std::vector<TDD_Related> m_related;

public:      // methods
  ~TDD_Diagnostic();

  // Empty `m_related`.
  TDD_Diagnostic(std::string &&message);

  TDD_Diagnostic(std::string &&message, std::vector<TDD_Related> &&related);

  operator gdv::GDValue() const;

  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(TDD_Diagnostic)
};


// A set of `(TextMCoordRange, TDD_Diagnostic)` tuples, stored in a way
// that allows efficient incremental updating when the document changes.
//
// This class exposes methods by which incremental update can be
// effected, but does not do those updates itself because it is not
// associated with any specific document.  Instead,
// `TextDocumentDiagnosticsUpdater`, below, is what ties diagnostics and
// document together and performs the updates to keep them synchronized.
//
// However, the `adjustForDocument` method must be called before updates
// are tracked; prior to that, this class can hold the diagnostics, but
// cannot update them.
//
class TextDocumentDiagnostics : public SerfRefCount {
public:      // types
  // Type that acts as the value for the range map, and the index into
  // `m_diagnostics`.
  typedef TextMCoordMap::Value DiagnosticIndex;

  // One mapping, with document-wide boundary scope.  That is, this is
  // logically what this data structure contains a set of.
  class DocEntry {
  public:      // data
    // Range of text the diagnostic pertains to.
    TextMCoordRange m_range;

    // The diagnostic for that range.
    RCSerf<TDD_Diagnostic const> m_diagnostic;

  public:      // data
    ~DocEntry();
    DocEntry(TextMCoordRange range, TDD_Diagnostic const *diagnostic);

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(DocEntry)
  };

  // A description of the portion of a line that intersects a
  // diagnostic.
  class LineEntry {
  public:      // data
    // If set, the index where the range starts on this line.  If not
    // set, the range begins on a previous line.
    std::optional<ByteIndex> m_startByteIndex;

    // If set, the index where the range ends on this line.  If not set,
    // the range ends on a subsequent line.
    //
    // Invariant: If both indices are set, then start <= end.
    std::optional<ByteIndex> m_endByteIndex;

    // The associated diagnostic.  This is a pointer into
    // `m_diagnostics`, so is invalidated by anything that changes that
    // set.
    RCSerf<TDD_Diagnostic const> m_diagnostic;

  public:      // methods
    ~LineEntry();

    explicit LineEntry(
      std::optional<ByteIndex> startByteIndex,
      std::optional<ByteIndex> endByteIndex,
      TDD_Diagnostic const *m_diagnostic);

    // Assert invariants.
    void selfCheck() const;

    // True if `byteIndex` is between start and end.
    bool containsByteIndex(ByteIndex byteIndex) const;

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineEntry)
  };

private:     // data
  // When the diagnostics were originally received, they described this
  // version of the document.
  TD_VersionNumber const m_originVersion;

  // Set of diagnostics, organized into a sequence so each has a unique
  // index that can be used with `m_rangeToDiagIndex`.
  std::vector<TDD_Diagnostic> m_diagnostics;

  // Map a coordinate range to an index into `m_diagnostics`.
  //
  // Invariant: The set of values in `m_rangeToDiagIndex` is
  // [0, m_diagnostics.size()-1].
  TextMCoordMap m_rangeToDiagIndex;

public:      // methods
  ~TextDocumentDiagnostics();

  TextDocumentDiagnostics(TextDocumentDiagnostics const &obj);

  // Make an initially empty set of diagnostics.
  //
  // `originVersion` is the version number of the document with which
  // these diagnostics are associated.
  //
  // `numLines` is the number of lines (newline characters plus one) in
  // the associated document, if that is known.
  explicit TextDocumentDiagnostics(
    TD_VersionNumber originVersion, std::optional<PositiveLineCount> numLines);

  // Assert all invariants.
  void selfCheck() const;

  bool operator==(TextDocumentDiagnostics const &obj) const;
  bool operator!=(TextDocumentDiagnostics const &obj) const
    { return !operator==(obj); }

  TD_VersionNumber getOriginVersion() const { return m_originVersion; }

  // Number of lines in the document the diagnostics apply to.
  std::optional<PositiveLineCount> getNumLinesOpt() const;

  // True if there are no mappings.
  bool empty() const;

  // Number of mappings.
  std::size_t size() const;

  // If there are no diagnostics, this returns -1.  Otherwise, it is the
  // largest line number for which there is any intersecting diagnostic.
  int maxDiagnosticLine() const;

  // The number of lines that potentially have associated diagnostic
  // data.  Numerically, this is `maxDiagnosticLine() + 1`.
  LineCount numLinesWithData() const;

  // Remove all diagnostics and reset to `numLines`, which must be
  // positive.
  void clearEverything(PositiveLineCount numLines);

  // Insert the mapping `range` -> `diag`.
  void insertDiagnostic(TextMCoordRange range, TDD_Diagnostic &&diag);

  // Return all diagnostic entries that intersect `line`.
  std::set<LineEntry> getLineEntries(LineIndex line) const;

  // Return all entries for the entire document.
  std::set<DocEntry> getAllEntries() const;

  // If there is a diagnostic containing `coord`, return a pointer to
  // it.  This pointer becomes invalid if the diagnostics change, so
  // must be immediately used and then discarded.  If there is none,
  // return a null pointer.  If there is more than one, first prefer one
  // with a closer start, then a closer end, then resolve arbitrarily.
  RCSerf<TDD_Diagnostic const> getDiagnosticAt(TextMCoord tc) const;

  // If there is a diagnostic that starts after `tc`, return the start
  // location of the one closest to `tc`; otherwise nullopt.
  std::optional<TextMCoord> getNextDiagnosticLocation(
    TextMCoord tc) const;

  // Same for starting before `tc`.
  std::optional<TextMCoord> getPreviousDiagnosticLocation(
    TextMCoord tc) const;

  // Do "next" or "previous" depending on `next`.
  std::optional<TextMCoord> getAdjacentDiagnosticLocation(
    bool next, TextMCoord tc) const;

  // Set the line count and confine line indices accordingly.  This is
  // normally done before `adjustForDocument`.
  void setNumLinesAndAdjustAccordingly(PositiveLineCount numLines);

  // Adjust all diagnostic ranges to be valid for `doc`.  This is meant
  // to be used when a set of diagnostics is received from some external
  // source (like compiler error messages) and we want to bind them to a
  // document.  Since the incoming diagnostics could have any locations,
  // this procedure forcibly confines them to the current document
  // shape, thus establishing the correspondence invariant that
  // `TextDocumentDiagnosticsUpdater` can then maintain going forward.
  void adjustForDocument(TextDocumentCore const &doc);

  // Perform updates on the underlying mapping in order to track text
  // updates.  These have the same semantics as the same-named methods
  // on `TextMCoordMap`.
  void insertLines(LineIndex line, LineCount count);
  void deleteLines(LineIndex line, LineCount count);
  void insertLineBytes(TextMCoord tc, ByteCount lengthBytes);
  void deleteLineBytes(TextMCoord tc, ByteCount lengthBytes);

  // Equivalent to `toGDValue(getAllEntries())`.
  operator gdv::GDValue() const;

  // Apply `change`.
  void applyDocumentChange(TextDocumentChange const &change);

  // Apply every change in `seq`.
  void applyDocumentChangeSequence(
    TextDocumentChangeSequence const &seq);
};


// An object that watches a particular document for changes and updates
// a set of diagnostics accordingly.
class TextDocumentDiagnosticsUpdater : public TextDocumentObserver {
  // Would be unsafe due to add/remove observer.
  NO_OBJECT_COPIES(TextDocumentDiagnosticsUpdater);

private:     // data
  // The set of diagnostics we will update when `m_doc` changes.
  //
  // Invariant: Every range consists of valid model coordinates for
  // `m_doc` with start <= end.
  RCSerf<TextDocumentDiagnostics> const m_diagnostics;

  // The document we are watching.
  //
  // We watch it, but do not modify it, hence the inner `const`.  (In
  // the `NamedTextDocument` API, adding or removing an observer is
  // considered a const-compatible action.)
  RCSerf<NamedTextDocument const> const m_document;

public:      // methods
  ~TextDocumentDiagnosticsUpdater();

  // `diagnostics` must already describe ranges that are valid for
  // `doc`.  This can be done by calling
  // `diagnostics->adjustForDocument(document->getCore())` ahead of
  // time.
  explicit TextDocumentDiagnosticsUpdater(
    TextDocumentDiagnostics *diagnostics,
    NamedTextDocument const *document);

  void selfCheck() const;

  // Return the constructor parameters.
  TextDocumentDiagnostics *getDiagnostics() const;
  NamedTextDocument const *getDocument() const;

public:      // TextDocumentObserver methods
  // Each of these calls corresponding methods on `m_diagnostics` to
  // keep them up to date, i.e., so each diagnostic continues to apply
  // to the "same" range of text.  They also maintain the number of
  // lines.
  virtual void observeInsertLine(TextDocumentCore const &doc, LineIndex line) noexcept override;
  virtual void observeDeleteLine(TextDocumentCore const &doc, LineIndex line) noexcept override;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, ByteCount lengthBytes) noexcept override;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, ByteCount lengthBytes) noexcept override;

  // This clears the diagnostics and resets the number of lines to match
  // `doc`.
  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept override;
};


// The test code, `td-diagnostics-test.cc`, defines
// `TextDocumentDiagnosticsAndUpdater`, which could be moved here if
// there is a need for it.


#endif // EDITOR_TD_DIAGNOSTICS_H
