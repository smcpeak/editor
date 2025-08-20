// td-diagnostics.cc
// Code for `td-diagnostics` module.

// See license.txt for copyright and terms of use.

#include "smbase/gdvalue-optional-fwd.h"         // gdv::GDValue(std::optional)
#include "smbase/gdvalue-set-fwd.h"              // gdv::GDValue(std::set)
#include "smbase/gdvalue-vector-fwd.h"           // gdv::GDValue(std::vector)

#include "td-diagnostics.h"                      // this module

#include "named-td.h"                            // NamedTextDocument
#include "td-change.h"                           // TextDocumentChange
#include "td-change-seq.h"                       // TextDocumentChangeSequence
#include "textmcoord-map.h"                      // TextMCoordMap

#include "smbase/ast-switch.h"                   // ASTSWITCHC
#include "smbase/compare-util.h"                 // smbase::compare
#include "smbase/container-util.h"               // smbase::reverseIterRange
#include "smbase/gdvalue.h"                      // gdv::GDValue
#include "smbase/gdvalue-optional.h"             // gdv::GDValue(std::optional)
#include "smbase/gdvalue-set.h"                  // gdv::GDValue(std::set)
#include "smbase/gdvalue-vector.h"               // gdv::GDValue(std::vector)
#include "smbase/overflow.h"                     // convertNumber
#include "smbase/sm-macros.h"                    // IMEMBFP, IMEMBMFP, DMEMB

#include <optional>                              // std::optional
#include <string>                                // std::string
#include <utility>                               // std::move

using namespace gdv;
using namespace smbase;


// ---------------------------- TDD_Related ----------------------------
TDD_Related::~TDD_Related()
{}


TDD_Related::TDD_Related(std::string &&file, int line, std::string &&message)
  : IMEMBMFP(file),
    IMEMBMFP(line),
    IMEMBMFP(message)
{}


TDD_Related::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TDD_Related"_sym);

  GDV_WRITE_MEMBER_SYM(m_file);
  GDV_WRITE_MEMBER_SYM(m_line);
  GDV_WRITE_MEMBER_SYM(m_message);

  return m;
}


int TDD_Related::compareTo(TDD_Related const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_file);
  RET_IF_COMPARE_MEMBERS(m_line);
  RET_IF_COMPARE_MEMBERS(m_message);
  return 0;
}


// -------------------------- TDD_Diagnostic ---------------------------
TDD_Diagnostic::~TDD_Diagnostic()
{}


TDD_Diagnostic::TDD_Diagnostic(std::string &&message)
  : IMEMBMFP(message),
    m_related()
{}


TDD_Diagnostic::TDD_Diagnostic(std::string &&message,
                       std::vector<TDD_Related> &&related)
  : IMEMBMFP(message),
    IMEMBMFP(related)
{}


int TDD_Diagnostic::compareTo(TDD_Diagnostic const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_message);
  RET_IF_COMPARE_MEMBERS(m_related);
  return 0;
}


TDD_Diagnostic::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TDD_Diagnostic"_sym);

  GDV_WRITE_MEMBER_SYM(m_message);
  GDV_WRITE_MEMBER_SYM(m_related);

  return m;
}



// ----------------------------- DocEntry ------------------------------
TextDocumentDiagnostics::DocEntry::~DocEntry()
{}


TextDocumentDiagnostics::DocEntry::DocEntry(
  TextMCoordRange range,
  TDD_Diagnostic const *diagnostic)
  : m_range(range),
    m_diagnostic(diagnostic)
{}


int TextDocumentDiagnostics::DocEntry::compareTo(DocEntry const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_range);
  RET_IF_NONZERO(deepCompare(a.m_diagnostic, b.m_diagnostic));
  return 0;
}


TextDocumentDiagnostics::DocEntry::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TDD_DocEntry"_sym);

  GDV_WRITE_MEMBER_SYM(m_range);
  m.mapSetValueAtSym("diagnostic", toGDValue(*m_diagnostic));

  return m;
}


// ----------------------------- LineEntry -----------------------------
TextDocumentDiagnostics::LineEntry::~LineEntry()
{}


TextDocumentDiagnostics::LineEntry::LineEntry(
  std::optional<int> startByteIndex,
  std::optional<int> endByteIndex,
  TDD_Diagnostic const *m_diagnostic)
  : m_startByteIndex(startByteIndex),
    m_endByteIndex(endByteIndex),
    m_diagnostic(m_diagnostic)
{
  selfCheck();
}


void TextDocumentDiagnostics::LineEntry::selfCheck() const
{
  if (m_startByteIndex && m_endByteIndex) {
    xassert(*m_startByteIndex <= *m_endByteIndex);
  }
}


bool TextDocumentDiagnostics::LineEntry::containsByteIndex(int byteIndex) const
{
  if (m_startByteIndex && *m_startByteIndex > byteIndex) {
    return false;
  }

  if (m_endByteIndex && *m_endByteIndex <= byteIndex) {
    if (*m_endByteIndex == byteIndex &&
        m_startByteIndex &&
        *m_startByteIndex == byteIndex) {
      // As a special case, for a collapsed range, say it contains the
      // index at the shared endpoint, since otherwise there would be no
      // location that it contains, and hence no way to see the message
      // in the UI (the way I'm thinking of showing them).
      return true;
    }

    return false;
  }

  return true;
}


TextDocumentDiagnostics::LineEntry::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TDD_LineEntry"_sym);

  GDV_WRITE_MEMBER_SYM(m_startByteIndex);
  GDV_WRITE_MEMBER_SYM(m_endByteIndex);
  m.mapSetValueAtSym("diagnostic", toGDValue(*m_diagnostic));

  return m;
}


int TextDocumentDiagnostics::LineEntry::compareTo(LineEntry const &b) const
{
  auto const &a = *this;

  // Smaller start is less.
  RET_IF_COMPARE_MEMBERS(m_startByteIndex);

  // *Larger* end is less, since I want smaller ranges to be considered
  // to come after larger ranges, since then I can say that a range that
  // compares as greater is "better" in the context of
  // `getDiagnosticAt`.
  //
  // But, semantically, for the end point, an absent value should be
  // treated as numerically larger (and hence order-wise smaller) than a
  // present endpoint.
  RET_IF_NONZERO(compare(a.m_endByteIndex.has_value(),
                         b.m_endByteIndex.has_value()));

  if (a.m_endByteIndex.has_value()) {
    // Both are present, so flip the order here.
    RET_IF_NONZERO(compare(b.m_endByteIndex.value(),
                           a.m_endByteIndex.value()));
  }

  RET_IF_NONZERO(deepCompare(a.m_diagnostic, b.m_diagnostic));
  return 0;
}


// ---------------------- TextDocumentDiagnostics ----------------------
TextDocumentDiagnostics::~TextDocumentDiagnostics()
{}


TextDocumentDiagnostics::TextDocumentDiagnostics(
  TextDocumentDiagnostics const &obj)
  : DMEMB(m_originVersion),
    DMEMB(m_diagnostics),
    DMEMB(m_rangeToDiagIndex)
{
  selfCheck();
}


TextDocumentDiagnostics::TextDocumentDiagnostics(
  VersionNumber originVersion,
  std::optional<int> numLines)
  : IMEMBFP(originVersion),
    m_diagnostics(),
    m_rangeToDiagIndex(numLines)
{
  selfCheck();
}


void TextDocumentDiagnostics::selfCheck() const
{
  std::set<DiagnosticIndex> vectorIndices;
  for (DiagnosticIndex i=0; (std::size_t)i < m_diagnostics.size(); ++i) {
    vectorIndices.insert(i);
  }

  std::set<DiagnosticIndex> mapIndices =
    m_rangeToDiagIndex.getMappedValues();

  xassert(vectorIndices == mapIndices);
}


bool TextDocumentDiagnostics::operator==(
  TextDocumentDiagnostics const &obj) const
{
  return EMEMB(m_originVersion) &&
         EMEMB(m_diagnostics) &&
         EMEMB(m_rangeToDiagIndex);
}


std::optional<int> TextDocumentDiagnostics::getNumLinesOpt() const
{
  return m_rangeToDiagIndex.getNumLinesOpt();
}


bool TextDocumentDiagnostics::empty() const
{
  return m_diagnostics.empty();
}


std::size_t TextDocumentDiagnostics::size() const
{
  return m_diagnostics.size();
}


int TextDocumentDiagnostics::maxDiagnosticLine() const
{
  return m_rangeToDiagIndex.maxEntryLine();
}


void TextDocumentDiagnostics::clearEverything(int numLines)
{
  xassertPrecondition(numLines > 0);

  m_diagnostics.clear();
  m_rangeToDiagIndex.clearEverything(numLines);
}


void TextDocumentDiagnostics::insertDiagnostic(
  TextMCoordRange range, TDD_Diagnostic &&diag)
{
  DiagnosticIndex index =
    convertNumber<DiagnosticIndex>(m_diagnostics.size());
  m_diagnostics.push_back(std::move(diag));
  m_rangeToDiagIndex.insertEntry({range, index});
}


auto TextDocumentDiagnostics::getLineEntries(int line) const
  -> std::set<LineEntry>
{
  std::set<LineEntry> ret;

  std::set<TextMCoordMap::LineEntry> underEntries =
    m_rangeToDiagIndex.getLineEntries(line);

  for (auto const &underEntry : underEntries) {
    ret.insert(LineEntry(
      underEntry.m_startByteIndex,
      underEntry.m_endByteIndex,
      &( m_diagnostics.at(underEntry.m_value) )
    ));
  }

  return ret;
}


auto TextDocumentDiagnostics::getAllEntries() const
  -> std::set<DocEntry>
{
  std::set<DocEntry> ret;

  std::set<TextMCoordMap::DocEntry> underEntries =
    m_rangeToDiagIndex.getAllEntries();

  for (auto const &underEntry : underEntries) {
    ret.insert(DocEntry(
      underEntry.m_range,
      &( m_diagnostics.at(underEntry.m_value) )
    ));
  }

  return ret;
}


auto TextDocumentDiagnostics::getDiagnosticAt(TextMCoord tc) const
  -> RCSerf<TDD_Diagnostic const>
{
  std::set<LineEntry> lineEntries = getLineEntries(tc.m_line);

  std::optional<LineEntry> best;

  // Get the last that contains `tc`; the comparison order of
  // `LineEntry` has been designed specifically to work as desired in
  // this context.
  for (LineEntry const &entry : lineEntries) {
    if (entry.containsByteIndex(tc.m_byteIndex)) {
      best = entry;
    }
  }

  if (best) {
    return best->m_diagnostic;
  }

  // Not found.
  return {};
}


std::optional<TextMCoord>
TextDocumentDiagnostics::getNextDiagnosticLocation(TextMCoord tc) const
{
  // This somewhat naively searches all lines starting with `tc.m_line`.
  // It could be more efficient by taking advantage of the line array
  // inside `m_rangeToDiagIndex`.

  int maxLine = maxDiagnosticLine();
  for (int line = tc.m_line; line <= maxLine; ++line) {
    std::set<LineEntry> lineEntries = getLineEntries(line);
    for (LineEntry const &entry : lineEntries) {
      if (entry.m_startByteIndex.has_value() &&
          (line > tc.m_line ||
           *entry.m_startByteIndex > tc.m_byteIndex)) {
        // The line entries are in order of `m_startByteIndex`, so this
        // must be the first that is greater than `tc`.
        return TextMCoord(line, *entry.m_startByteIndex);
      }
    }
  }

  return {};
}


std::optional<TextMCoord>
TextDocumentDiagnostics::getPreviousDiagnosticLocation(TextMCoord tc) const
{
  for (int line = tc.m_line; line >= 0; --line) {
    std::set<LineEntry> lineEntries = getLineEntries(line);
    for (LineEntry const &entry : reverseIterRange(lineEntries)) {
      if (entry.m_startByteIndex.has_value() &&
          (line < tc.m_line ||
           *entry.m_startByteIndex < tc.m_byteIndex)) {
        return TextMCoord(line, *entry.m_startByteIndex);
      }
    }
  }

  return {};
}


std::optional<TextMCoord>
TextDocumentDiagnostics::getAdjacentDiagnosticLocation(
  bool next, TextMCoord tc) const
{
  return next?
    getNextDiagnosticLocation(tc) :
    getPreviousDiagnosticLocation(tc);
}


void TextDocumentDiagnostics::setNumLinesAndAdjustAccordingly(
  int numLines)
{
  m_rangeToDiagIndex.setNumLinesAndAdjustAccordingly(numLines);
}


void TextDocumentDiagnostics::adjustForDocument(
  TextDocumentCore const &doc)
{
  m_rangeToDiagIndex.adjustForDocument(doc);
}


void TextDocumentDiagnostics::insertLines(int line, int count)
{
  m_rangeToDiagIndex.insertLines(line, count);
}

void TextDocumentDiagnostics::deleteLines(int line, int count)
{
  m_rangeToDiagIndex.deleteLines(line, count);
}

void TextDocumentDiagnostics::insertLineBytes(TextMCoord tc, int lengthBytes)
{
  m_rangeToDiagIndex.insertLineBytes(tc, lengthBytes);
}

void TextDocumentDiagnostics::deleteLineBytes(TextMCoord tc, int lengthBytes)
{
  m_rangeToDiagIndex.deleteLineBytes(tc, lengthBytes);
}


TextDocumentDiagnostics::operator gdv::GDValue() const
{
  return toGDValue(getAllEntries());
}


void TextDocumentDiagnostics::applyDocumentChange(
  TextDocumentChange const &change)
{
  ASTSWITCHC(TextDocumentChange, &change) {
    ASTCASEC(TDC_InsertLine, insertLine) {
      this->insertLines(insertLine->m_line, 1);
    }

    ASTNEXTC(TDC_DeleteLine, deleteLine) {
      this->deleteLines(deleteLine->m_line, 1);
    }

    ASTNEXTC(TDC_InsertText, insertText) {
      this->insertLineBytes(insertText->m_tc,
                            insertText->m_text.size());
    }

    ASTNEXTC(TDC_DeleteText, deleteText) {
      this->deleteLineBytes(deleteText->m_tc,
                            deleteText->m_lengthBytes);
    }

    ASTNEXTC(TDC_TotalChange, totalChange) {
      this->clearEverything(totalChange->m_numLines);
    }

    ASTENDCASEC
  }
}


void TextDocumentDiagnostics::applyDocumentChangeSequence(
  TextDocumentChangeSequence const &seq)
{
  for (std::unique_ptr<TextDocumentChange> const &changePtr :
         seq.m_seq) {
    applyDocumentChange(*changePtr);
  }
}


// ------------------ TextDocumentDiagnosticsUpdater -------------------
TextDocumentDiagnosticsUpdater::~TextDocumentDiagnosticsUpdater()
{
  m_document->removeObserver(this);
}


TextDocumentDiagnosticsUpdater::TextDocumentDiagnosticsUpdater(
  TextDocumentDiagnostics *diagnostics,
  NamedTextDocument const *document)
  : IMEMBFP(diagnostics),
    IMEMBFP(document)
{
  selfCheck();

  m_document->addObserver(this);
}


void TextDocumentDiagnosticsUpdater::selfCheck() const
{
  std::set<TextDocumentDiagnostics::DocEntry> entries =
    m_diagnostics->getAllEntries();

  for (auto const &entry : entries) {
    xassert(m_document->validRange(entry.m_range));
    xassert(entry.m_range.isRectified());
  }
}


TextDocumentDiagnostics *TextDocumentDiagnosticsUpdater::getDiagnostics() const
{
  return m_diagnostics;
}


NamedTextDocument const *TextDocumentDiagnosticsUpdater::getDocument() const
{
  return m_document;
}


void TextDocumentDiagnosticsUpdater::observeInsertLine(TextDocumentCore const &doc, int line) noexcept
{
  GENERIC_CATCH_BEGIN
  m_diagnostics->insertLines(line, 1);
  GENERIC_CATCH_END
}

void TextDocumentDiagnosticsUpdater::observeDeleteLine(TextDocumentCore const &doc, int line) noexcept
{
  GENERIC_CATCH_BEGIN
  m_diagnostics->deleteLines(line, 1);
  GENERIC_CATCH_END
}

void TextDocumentDiagnosticsUpdater::observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept
{
  GENERIC_CATCH_BEGIN
  m_diagnostics->insertLineBytes(tc, lengthBytes);
  GENERIC_CATCH_END
}

void TextDocumentDiagnosticsUpdater::observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept
{
  GENERIC_CATCH_BEGIN
  m_diagnostics->deleteLineBytes(tc, lengthBytes);
  GENERIC_CATCH_END
}

void TextDocumentDiagnosticsUpdater::observeTotalChange(TextDocumentCore const &doc) noexcept
{
  GENERIC_CATCH_BEGIN
  m_diagnostics->clearEverything(doc.numLines());
  GENERIC_CATCH_END
}


// EOF
