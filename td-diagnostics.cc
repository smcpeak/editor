// td-diagnostics.cc
// Code for `td-diagnostics` module.

// See license.txt for copyright and terms of use.

#include "td-diagnostics.h"            // this module

#include "named-td.h"                  // NamedTextDocument
#include "textmcoord-map.h"            // TextMCoordMap

#include "smbase/compare-util.h"       // smbase::compare
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/overflow.h"           // convertNumber

#include <optional>                    // std::optional
#include <string>                      // std::string
#include <utility>                     // std::move

using namespace gdv;
using namespace smbase;


// ---------------------------- Diagnostic -----------------------------
TextDocumentDiagnostics::Diagnostic::~Diagnostic()
{}


TextDocumentDiagnostics::Diagnostic::Diagnostic(std::string &&message)
  : m_message(std::move(message))
{}


int TextDocumentDiagnostics::Diagnostic::compareTo(Diagnostic const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_message);
  return 0;
}


// ----------------------------- LineEntry -----------------------------
TextDocumentDiagnostics::LineEntry::~LineEntry()
{}


TextDocumentDiagnostics::LineEntry::LineEntry(
  std::optional<int> startByteIndex,
  std::optional<int> endByteIndex,
  Diagnostic const *m_diagnostic)
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


int TextDocumentDiagnostics::LineEntry::compareTo(LineEntry const &b) const
{
  auto const &a = *this;
  RET_IF_COMPARE_MEMBERS(m_startByteIndex);
  RET_IF_COMPARE_MEMBERS(m_endByteIndex);

  // Distiguish by presence of a diagnostic.
  RET_IF_NONZERO(!!a.m_diagnostic - !!b.m_diagnostic);

  if (a.m_diagnostic) {
    // If both are present, use deep comparison.
    RET_IF_NONZERO(compare(*a.m_diagnostic, *b.m_diagnostic));
  }

  return 0;
}


// ---------------------- TextDocumentDiagnostics ----------------------
TextDocumentDiagnostics::~TextDocumentDiagnostics()
{
  m_doc->removeObserver(this);
}


TextDocumentDiagnostics::TextDocumentDiagnostics(NamedTextDocument *doc)
  : m_doc(doc),
    m_diagnostics(),
    m_rangeToDiagIndex()
{
  selfCheck();

  m_doc->addObserver(this);
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

  std::set<TextMCoordMap::Entry> underEntries =
    m_rangeToDiagIndex.getAllEntries();
  for (auto const &underEntry : underEntries) {
    xassert(m_doc->validRange(underEntry.m_range));
    xassert(underEntry.m_range.isRectified());
  }
}


bool TextDocumentDiagnostics::empty() const
{
  return m_diagnostics.empty();
}


std::size_t TextDocumentDiagnostics::size() const
{
  return m_diagnostics.size();
}


void TextDocumentDiagnostics::insert(TextMCoordRange range, Diagnostic &&diag)
{
  DiagnosticIndex index =
    convertNumber<DiagnosticIndex>(m_diagnostics.size());
  m_diagnostics.push_back(std::move(diag));
  m_rangeToDiagIndex.insert({range, index});
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


TextDocumentDiagnostics::operator gdv::GDValue() const
{
  // TODO: This should include the diagnostic messages.
  return toGDValue(m_rangeToDiagIndex);
}


void TextDocumentDiagnostics::observeInsertLine(TextDocumentCore const &doc, int line) noexcept
{
  m_rangeToDiagIndex.insertLines(line, 1);
}

void TextDocumentDiagnostics::observeDeleteLine(TextDocumentCore const &doc, int line) noexcept
{
  m_rangeToDiagIndex.deleteLines(line, 1);
}

void TextDocumentDiagnostics::observeInsertText(TextDocumentCore const &doc, TextMCoord tc, char const *text, int lengthBytes) noexcept
{
  m_rangeToDiagIndex.insertLineBytes(tc, lengthBytes);
}

void TextDocumentDiagnostics::observeDeleteText(TextDocumentCore const &doc, TextMCoord tc, int lengthBytes) noexcept
{
  m_rangeToDiagIndex.deleteLineBytes(tc, lengthBytes);
}

void TextDocumentDiagnostics::observeTotalChange(TextDocumentCore const &doc) noexcept
{
  m_rangeToDiagIndex.clear();
}


// EOF
