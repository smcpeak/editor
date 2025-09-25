// tdd-proposed-fix.h
// `TDD_ProposedFix`, a fix proposed for some code issue.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TDD_PROPOSED_FIX_H
#define EDITOR_TDD_PROPOSED_FIX_H

#include "tdd-proposed-fix-fwd.h"      // fwds for this module

#include "textmcoord.h"                // TextMCoordRange

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/std-vector-fwd.h"     // stdfwd::vector [n]

#include <string>                      // std::string
#include <map>                         // std::map


// One of a sequence of edits to perform to some file.
class TDD_TextEdit final {
public:      // data
  // Range to modify.
  TextMCoordRange m_range;

  // New text to replace whatever is in the range.
  std::string m_newText;

public:      // methods
  // create-tuple-class: declarations for TDD_TextEdit +move +gdvWrite +compare +gdvRead
  /*AUTO_CTC*/ explicit TDD_TextEdit(TextMCoordRange const &range, std::string const &newText);
  /*AUTO_CTC*/ explicit TDD_TextEdit(TextMCoordRange &&range, std::string &&newText);
  /*AUTO_CTC*/ TDD_TextEdit(TDD_TextEdit const &obj) noexcept;
  /*AUTO_CTC*/ TDD_TextEdit(TDD_TextEdit &&obj) noexcept;
  /*AUTO_CTC*/ TDD_TextEdit &operator=(TDD_TextEdit const &obj) noexcept;
  /*AUTO_CTC*/ TDD_TextEdit &operator=(TDD_TextEdit &&obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(TDD_TextEdit const &a, TDD_TextEdit const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(TDD_TextEdit)
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, TDD_TextEdit const &obj);
  /*AUTO_CTC*/ // For +gdvRead:
  /*AUTO_CTC*/ explicit TDD_TextEdit(gdv::GDValueParser const &p);
};


// A fix proposed to address some code issue.
class TDD_ProposedFix final {
public:      // data
  // Summary of the fix.  Example: "insert ';'".
  std::string m_title;

  // For each file, a sequence of changes to apply.
  std::map<std::string, stdfwd::vector<TDD_TextEdit>> m_changesForFile;

public:      // methods
  // create-tuple-class: declarations for TDD_ProposedFix +move +gdvWrite +compare +gdvRead
  /*AUTO_CTC*/ explicit TDD_ProposedFix(std::string const &title, std::map<std::string, stdfwd::vector<TDD_TextEdit>> const &changesForFile);
  /*AUTO_CTC*/ explicit TDD_ProposedFix(std::string &&title, std::map<std::string, stdfwd::vector<TDD_TextEdit>> &&changesForFile);
  /*AUTO_CTC*/ TDD_ProposedFix(TDD_ProposedFix const &obj) noexcept;
  /*AUTO_CTC*/ TDD_ProposedFix(TDD_ProposedFix &&obj) noexcept;
  /*AUTO_CTC*/ TDD_ProposedFix &operator=(TDD_ProposedFix const &obj) noexcept;
  /*AUTO_CTC*/ TDD_ProposedFix &operator=(TDD_ProposedFix &&obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(TDD_ProposedFix const &a, TDD_ProposedFix const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(TDD_ProposedFix)
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, TDD_ProposedFix const &obj);
  /*AUTO_CTC*/ // For +gdvRead:
  /*AUTO_CTC*/ explicit TDD_ProposedFix(gdv::GDValueParser const &p);

  // Number of files affected.
  int numFiles() const;

  // Total number of edits across all files.
  int numEdits() const;
};


#endif // EDITOR_TDD_PROPOSED_FIX_H
