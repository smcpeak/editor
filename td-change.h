// td-change.h
// `TextDocumentChange` class hierarchy.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TD_CHANGE_H
#define EDITOR_TD_CHANGE_H

#include "td-change-fwd.h"             // fwds for this module

#include "line-index.h"                // LineIndex
#include "range-text-repl-fwd.h"       // RangeTextReplacement [n]
#include "td-core-fwd.h"               // TextDocumentCore [n]
#include "textmcoord.h"                // TextMCoord

#include "smbase/ast-switch.h"         // DECL_AST_DOWNCASTS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue

#include <optional>                    // std::optional
#include <string>                      // std::string


// A record of one of the changes that can be observed via the
// `TextDocumentObserver` interface.
class TextDocumentChange {
public:      // types
  // Enumeration of all concrete `TextDocumentChange`
  // subclasses.
  enum Kind {
    K_INSERT_LINE,
    K_DELETE_LINE,
    K_INSERT_TEXT,
    K_DELETE_TEXT,
    K_TOTAL_CHANGE
  };

public:      // methods
  virtual ~TextDocumentChange();

  explicit TextDocumentChange();

  // Specific subclass.
  virtual Kind kind() const = 0;

  DECL_AST_DOWNCASTS(TDC_InsertLine, K_INSERT_LINE)
  DECL_AST_DOWNCASTS(TDC_DeleteLine, K_DELETE_LINE)
  DECL_AST_DOWNCASTS(TDC_InsertText, K_INSERT_TEXT)
  DECL_AST_DOWNCASTS(TDC_DeleteText, K_DELETE_TEXT)
  DECL_AST_DOWNCASTS(TDC_TotalChange, K_TOTAL_CHANGE)

  // Apply this change to `doc`.
  virtual void applyToDoc(TextDocumentCore &doc) const = 0;

  // Express as a range text replacement.
  virtual RangeTextReplacement getRangeTextReplacement() const = 0;

  // Dump data for testing and debugging.
  virtual operator gdv::GDValue() const = 0;
};


// Records `observeInsertLine`.
class TDC_InsertLine : public TextDocumentChange {
public:      // data
  // Observer method arguments.
  LineIndex m_line;

  // If set, then `m_line` is to become the new last line in the
  // document.  In order to express this as a range replacement, we need
  // to know the length of the previous line in bytes.
  std::optional<int> m_prevLineBytes;

public:      // methods
  virtual ~TDC_InsertLine() override;

  explicit TDC_InsertLine(LineIndex line, std::optional<int> prevLineBytes);

  static Kind constexpr TYPE_TAG = K_INSERT_LINE;
  virtual Kind kind() const override { return TYPE_TAG; }

  virtual void applyToDoc(TextDocumentCore &doc) const override;
  virtual RangeTextReplacement getRangeTextReplacement() const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeDeleteLine`.
class TDC_DeleteLine : public TextDocumentChange {
public:      // data
  // Observer method arguments.
  LineIndex m_line;

  // If set, then `m_line` is the last line in the document.  In order
  // to express this deletion as a range replacement, we need to know
  // the length of the previous line in bytes.
  std::optional<int> m_prevLineBytes;

public:      // methods
  virtual ~TDC_DeleteLine() override;

  explicit TDC_DeleteLine(LineIndex line, std::optional<int> prevLineBytes);

  static Kind constexpr TYPE_TAG = K_DELETE_LINE;
  virtual Kind kind() const override { return TYPE_TAG; }

  virtual void applyToDoc(TextDocumentCore &doc) const override;
  virtual RangeTextReplacement getRangeTextReplacement() const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeInsertText`.
class TDC_InsertText : public TextDocumentChange {
public:      // data
  // Observer method arguments.
  TextMCoord m_tc;

  // Although not needed for replaying to diagnostics, this is needed
  // for incremental content update for LSP.
  //
  // The length of `m_text` is the original `lengthBytes` argument.
  std::string m_text;

public:      // methods
  virtual ~TDC_InsertText() override;

  explicit TDC_InsertText(
    TextMCoord tc, char const *text, int lengthBytes);
  explicit TDC_InsertText(
    TextMCoord tc, std::string const &&text);

  static Kind constexpr TYPE_TAG = K_INSERT_TEXT;
  virtual Kind kind() const override { return TYPE_TAG; }

  virtual void applyToDoc(TextDocumentCore &doc) const override;
  virtual RangeTextReplacement getRangeTextReplacement() const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeDeleteText`.
class TDC_DeleteText : public TextDocumentChange {
public:      // data
  // Observer method arguments.
  TextMCoord m_tc;
  int m_lengthBytes;

public:      // methods
  virtual ~TDC_DeleteText() override;

  explicit TDC_DeleteText(
    TextMCoord tc, int lengthBytes);

  static Kind constexpr TYPE_TAG = K_DELETE_TEXT;
  virtual Kind kind() const override { return TYPE_TAG; }

  virtual void applyToDoc(TextDocumentCore &doc) const override;
  virtual RangeTextReplacement getRangeTextReplacement() const override;
  virtual operator gdv::GDValue() const override;
};


// Records `observeTotalChange`.
class TDC_TotalChange : public TextDocumentChange {
public:      // data
  // Number of lines in the document after the change.
  int m_numLines;

  // Full contents.
  std::string m_contents;

public:      // methods
  virtual ~TDC_TotalChange() override;

  explicit TDC_TotalChange(int m_numLines, std::string &&contents);

  static Kind constexpr TYPE_TAG = K_TOTAL_CHANGE;
  virtual Kind kind() const override { return TYPE_TAG; }

  virtual void applyToDoc(TextDocumentCore &doc) const override;
  virtual RangeTextReplacement getRangeTextReplacement() const override;
  virtual operator gdv::GDValue() const override;
};


#endif // EDITOR_TD_CHANGE_H
