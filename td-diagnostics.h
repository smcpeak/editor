// td-diagnostics.h
// Classes for representing diagnostics associated with a text document.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TD_DIAGNOSTICS_H
#define EDITOR_TD_DIAGNOSTICS_H

#include "td-diagnostics-fwd.h"        // fwds for this module

#include "named-td-fwd.h"              // NamedTextDocument
#include "td-core.h"                   // TextDocumentObserver
#include "textmcoord-map.h"            // TextMCoordMap

#include "smbase/compare-util-iface.h" // DEFINE_FRIEND_RELATIONAL_OPERATORS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/refct-serf.h"         // RCSerf, SerfRefCount

#include <string>                      // std::string


// A single diagnostic message.
class Diagnostic : public SerfRefCount {
public:      // data
  // What the diagnostic says.
  std::string m_message;

public:      // methods
  ~Diagnostic();
  Diagnostic(std::string &&message);

  operator gdv::GDValue() const;

  DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(Diagnostic)
};


// A set of diagnostics associated with one text document.
class TextDocumentDiagnostics : public TextDocumentObserver {
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
    RCSerf<Diagnostic const> m_diagnostic;

  public:      // data
    ~DocEntry();
    DocEntry(TextMCoordRange range, Diagnostic const *diagnostic);

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(DocEntry)
  };

  // A description of the portion of a line that intersects a
  // diagnostic.
  class LineEntry {
  public:      // data
    // If set, the index where the range starts on this line.  If not
    // set, the range begins on a previous line.
    std::optional<int> m_startByteIndex;

    // If set, the index where the range ends on this line.  If not set,
    // the range ends on a subsequent line.
    //
    // Invariant: If both indices are set, then start <= end.
    std::optional<int> m_endByteIndex;

    // The associated diagnostic.  This is a pointer into
    // `m_diagnostics`, so is invalidated by anything that changes that
    // set.
    RCSerf<Diagnostic const> m_diagnostic;

  public:      // methods
    ~LineEntry();

    explicit LineEntry(
      std::optional<int> startByteIndex,
      std::optional<int> endByteIndex,
      Diagnostic const *m_diagnostic);

    // Assert invariants.
    void selfCheck() const;

    // True if `byteIndex` is between start and end.
    bool containsByteIndex(int byteIndex) const;

    operator gdv::GDValue() const;

    DECLARE_COMPARETO_AND_DEFINE_RELATIONALS(LineEntry)
  };

private:     // data
  // Pointer to the document the diagnostics apply to.  We act as an
  // observer of `m_doc` in order to update the diagnostic locations
  // when the document changes.
  RCSerf<NamedTextDocument> m_doc;

  // Set of diagnostics, organized into a sequence so each has a unique
  // index that can be used with `m_rangeToDiagIndex`.
  std::vector<Diagnostic> m_diagnostics;

  // Map a coordinate range to an index into `m_diagnostics`.
  //
  // Invariant: The set of values in `m_rangeToDiagIndex` is
  // [0, m_diagnostics.size()-1].
  //
  // Invariant: Every range consists of valid model coordinates for
  // `m_doc` with start <= end.
  TextMCoordMap m_rangeToDiagIndex;

public:      // methods
  ~TextDocumentDiagnostics();

  // Initially empty set.
  explicit TextDocumentDiagnostics(NamedTextDocument *doc);

  // Assert all invariants.
  void selfCheck() const;

  // True if there are no mappings.
  bool empty() const;

  // Number of mappings.
  std::size_t size() const;

  // Insert the mapping `range` -> `diag`.
  void insert(TextMCoordRange range, Diagnostic &&diag);

  // Insert `range` -> `diag`, except first adjust `range` to be valid
  // for the document using `TextDocument::adjustMCoordRange`, returning
  // true and updating `range` if a change is made to it.
  bool insertWithAdjust(
    TextMCoordRange /*INOUT*/ &range, Diagnostic &&diag);

  // Return all diagnostic entries that intersect `line`.
  std::set<LineEntry> getLineEntries(int line) const;

  // Return all entries for the entire document.
  std::set<DocEntry> getAllEntries() const;

  // If there is a diagnostic containing `coord`, return a pointer to
  // it.  This pointer becomes invalid if the diagnostics change, so
  // must be immediately used and then discarded.  If there is none,
  // return a null pointer.  If there is more than one, first prefer one
  // with a closer start, then a closer end, then resolve arbitrarily.
  RCSerf<Diagnostic const> getDiagnosticAt(TextMCoord tc) const;

  operator gdv::GDValue() const;

public:      // TextDocumentObserver methods
  // Each of these adjusts the entries so that the "same" text is mapped
  // before and after.
  virtual void observeInsertLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeDeleteLine(TextDocumentCore const &doc, int line) noexcept override;
  virtual void observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept override;
  virtual void observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept override;

  // This clears the map.
  virtual void observeTotalChange(TextDocumentCore const &doc) noexcept override;
};


#endif // EDITOR_TD_DIAGNOSTICS_H
