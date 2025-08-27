// td-change.cc
// Code for `td-change` module.

#include "td-change.h"                 // this module

#include "td-core.h"                   // TextDocumentCore
#include "textmcoord.h"                // TextMCoord, TextMCoordRange
#include "range-text-repl.h"           // RangeTextReplacement

#include "smbase/ast-switch.h"         // DEFN_AST_DOWNCASTS
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/xassert.h"            // xassert

using namespace gdv;


// Return a range with size `n` at `pos`.
static TextMCoordRange rangeAtPlus(TextMCoord pos, int n)
{
  return TextMCoordRange(pos, pos.plusBytes(n));
}

static TextMCoordRange emptyRange(TextMCoord pos)
{
  return rangeAtPlus(pos, 0);
}


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
  LineIndex line, std::optional<int> prevLineBytes)
  : IMEMBFP(line),
    IMEMBFP(prevLineBytes)
{}


void TDC_InsertLine::applyToDoc(TextDocumentCore &doc) const
{
  doc.insertLine(m_line);
}


RangeTextReplacement TDC_InsertLine::getRangeTextReplacement() const
{
  // Normally we insert at the start of the line in question.
  TextMCoord pos(m_line, 0);

  if (m_prevLineBytes) {
    // But if we are appending a new line, then the position at
    // that line does not exist yet.  Append to the previous line
    // instead.
    pos = TextMCoord(m_line.pred(), *m_prevLineBytes);
  }

  return RangeTextReplacement(
    emptyRange(pos), std::string("\n"));
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
  LineIndex line, std::optional<int> prevLineBytes)
  : IMEMBFP(line),
    IMEMBFP(prevLineBytes)
{}


void TDC_DeleteLine::applyToDoc(TextDocumentCore &doc) const
{
  doc.deleteLine(m_line);
}


RangeTextReplacement TDC_DeleteLine::getRangeTextReplacement() const
{
  // Normally we delete the line by extending the range forward.
  TextMCoordRange range(
    TextMCoord(m_line, 0),
    TextMCoord(m_line.succ(), 0));

  if (m_prevLineBytes) {
    // But if it was the last line, going forward is a no-op, so
    // go backward instead.
    range = TextMCoordRange(
      TextMCoord(m_line.pred(), *m_prevLineBytes),
      TextMCoord(m_line, 0));
  }

  return RangeTextReplacement(range, std::string());
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


TDC_InsertText::TDC_InsertText(
  TextMCoord tc, std::string const &&text)
  : IMEMBFP(tc),
    IMEMBMFP(text)
{}


void TDC_InsertText::applyToDoc(TextDocumentCore &doc) const
{
  doc.insertString(m_tc, m_text);
}


RangeTextReplacement TDC_InsertText::getRangeTextReplacement() const
{
  return RangeTextReplacement(
    emptyRange(m_tc),
    m_text);
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


void TDC_DeleteText::applyToDoc(TextDocumentCore &doc) const
{
  doc.deleteTextBytes(m_tc, m_lengthBytes);
}


RangeTextReplacement TDC_DeleteText::getRangeTextReplacement() const
{
  return RangeTextReplacement(
    rangeAtPlus(m_tc, m_lengthBytes),
    std::string());
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


TDC_TotalChange::TDC_TotalChange(LineCount numLines, std::string &&contents)
  : IMEMBFP(numLines),
    IMEMBMFP(contents)
{}


void TDC_TotalChange::applyToDoc(TextDocumentCore &doc) const
{
  doc.replaceWholeFileString(m_contents);
}


RangeTextReplacement TDC_TotalChange::getRangeTextReplacement() const
{
  return RangeTextReplacement(
    std::nullopt,
    m_contents);
}


TDC_TotalChange::operator gdv::GDValue() const
{
  GDValue m(GDVK_TAGGED_ORDERED_MAP, "TotalChange"_sym);
  GDV_WRITE_MEMBER_SYM(m_numLines);
  GDV_WRITE_MEMBER_SYM(m_contents);
  return m;
}


// EOF
