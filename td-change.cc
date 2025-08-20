// td-change.cc
// Code for `td-change` module.

#include "td-change.h"                 // this module

#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/ast-switch.h"         // DEFN_AST_DOWNCASTS
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/xassert.h"            // xassert

using namespace gdv;


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


// EOF
