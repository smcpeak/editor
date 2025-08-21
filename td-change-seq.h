// td-change-seq.h
// `TextDocumentChangeSequence` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TD_CHANGE_SEQ_H
#define EDITOR_TD_CHANGE_SEQ_H

#include "td-change-seq-fwd.h"         // fwds for this module

#include "range-text-repl-fwd.h"       // RangeTextReplacement [n]
#include "td-change-fwd.h"             // TextDocumentChange [n]
#include "td-core-fwd.h"               // TextDocumentCore [n]
#include "td-fwd.h"                    // TextDocument [n]

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue [n]
#include "smbase/refct-serf.h"         // SerfRefCount
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr [n]

#include <cstddef>                     // std::size_t
#include <vector>                      // std::vector


// Sequence of changes.
class TextDocumentChangeSequence : public SerfRefCount {
  NO_OBJECT_COPIES(TextDocumentChangeSequence);

public:      // data
  // A sequence of changes that were applied to the document in the
  // order they happened.
  std::vector<stdfwd::unique_ptr<TextDocumentChange>>
    m_seq;

public:
  ~TextDocumentChangeSequence();

  // Initially empty sequence.
  TextDocumentChangeSequence();

  TextDocumentChangeSequence(TextDocumentChangeSequence &&obj);
  TextDocumentChangeSequence &operator=(TextDocumentChangeSequence &&obj);

  std::size_t size() const;

  operator gdv::GDValue() const;

  // Append `change` to the sequence.
  void append(stdfwd::unique_ptr<TextDocumentChange> change);

  // Apply `m_seq` to `doc`.
  void applyToDocCore(TextDocumentCore &doc) const;

  // Express as a sequence of range text replacements.
  std::vector<RangeTextReplacement> getRangeTextReplacements() const;

  // Apply `m_seq` to `doc`.
  void applyToDocument(TextDocument &doc) const;
};


// Randomly create a change that could be applied to `doc`.  This does
// not actually make the change.
//
// Usually this is just one change, but deleting a line requires first
// clearing it, so that is two changes.
TextDocumentChangeSequence makeRandomChange(
  TextDocumentCore const &doc);


#endif // EDITOR_TD_CHANGE_SEQ_H
