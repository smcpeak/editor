// td-change-seq.h
// `TextDocumentChangeSequence` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_TD_CHANGE_SEQ_H
#define EDITOR_TD_CHANGE_SEQ_H

#include "td-change-seq-fwd.h"         // fwds for this module

#include "td-change-fwd.h"             // TextDocumentChange

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/sm-macros.h"          // NO_OBJECT_COPIES
#include "smbase/std-memory-fwd.h"     // stdfwd::unique_ptr

#include <cstddef>                     // std::size_t
#include <vector>                      // std::vector


// Sequence of changes.
class TextDocumentChangeSequence {
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

  std::size_t size() const;

  operator gdv::GDValue() const;
};


#endif // EDITOR_TD_CHANGE_SEQ_H
