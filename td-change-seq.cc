// td-change-seq.cc
// Code for `td-change-seq` module.

#include "td-change-seq.h"             // this module

#include "range-text-repl.h"           // RangeTextReplacement
#include "td-change.h"                 // TextDocumentChange
#include "td-core.h"                   // TextDocumentCore
#include "td.h"                        // TextDocument

#include "smbase/gdvalue-unique-ptr.h" // gdv::toGDValue(std::unique_ptr)
#include "smbase/gdvalue-vector.h"     // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/sm-random.h"          // smbase::{sm_random, randomString{WithNL, NoNL}}
#include "smbase/swap-util.h"          // smbase::swapIfGreaterThan

#include <memory>                      // std::make_unique
#include <string>                      // std::string
#include <utility>                     // std::move

using namespace gdv;
using namespace smbase;


TextDocumentChangeSequence::~TextDocumentChangeSequence()
{}


TextDocumentChangeSequence::TextDocumentChangeSequence()
  : m_seq()
{}


TextDocumentChangeSequence::TextDocumentChangeSequence(
  TextDocumentChangeSequence &&obj)
  : MDMEMB(m_seq)
{}


TextDocumentChangeSequence &TextDocumentChangeSequence::operator=(
  TextDocumentChangeSequence &&obj)
{
  if (this != &obj) {
    MCMEMB(m_seq);
  }
  return *this;
}


std::size_t TextDocumentChangeSequence::size() const
{
  return m_seq.size();
}


TextDocumentChangeSequence::operator gdv::GDValue() const
{
  return toGDValue(m_seq);
}

void TextDocumentChangeSequence::append(
  std::unique_ptr<TextDocumentChange> change)
{
  m_seq.push_back(std::move(change));
}


void TextDocumentChangeSequence::applyToDocCore(
  TextDocumentCore &doc) const
{
  for (auto const &changePtr : m_seq) {
    changePtr->applyToDoc(doc);
  }
}

std::vector<RangeTextReplacement>
TextDocumentChangeSequence::getRangeTextReplacements() const
{
  std::vector<RangeTextReplacement> ret;

  for (auto const &changePtr : m_seq) {
    ret.push_back(changePtr->getRangeTextReplacement());
  }

  return ret;
}


void TextDocumentChangeSequence::applyToDocument(TextDocument &doc) const
{
  std::vector<RangeTextReplacement> replacements =
    getRangeTextReplacements();
  for (auto const &repl : replacements) {
    doc.applyRangeTextReplacement(repl);
  }
}


TextDocumentChangeSequence makeRandomChange(
  TextDocumentCore const &doc)
{
  TextDocumentChangeSequence seq;

  RandomChoice c(81);

  if (c.check(1)) {
    std::string newContents = randomStringWithNL(100);

    // For our purposes, newline characters *separate* lines.
    int numLines = numOccurrences(newContents, '\n') + 1;

    seq.append(std::make_unique<TDC_TotalChange>(
      numLines, std::move(newContents)));
  }

  else if (c.check(20)) {
    // Insert line.
    int line = sm_random(doc.numLines() + 1);

    std::optional<int> prevLineBytes;
    if (line == doc.numLines()) {
      prevLineBytes = doc.lineLengthBytes(line-1);
    }

    seq.append(std::make_unique<TDC_InsertLine>(
      line, prevLineBytes));
  }

  else if (c.check(20)) {
    // Delete line.
    if (doc.numLines() > 1) {
      int line = sm_random(doc.numLines());

      // First clear the line.
      seq.append(std::make_unique<TDC_DeleteText>(
        TextMCoord(line, 0), doc.lineLengthBytes(line)));

      // Then erase it.
      std::optional<int> prevLineBytes;
      if (line == doc.numLines() - 1) {
        prevLineBytes = doc.lineLengthBytes(line - 1);
      }
      seq.append(std::make_unique<TDC_DeleteLine>(
        line, prevLineBytes));
    }
    else {
      // Try again.
      seq = makeRandomChange(doc);
    }
  }

  else if (c.check(20)) {
    // Insert text.
    int line = sm_random(doc.numLines());
    int byteIndex = sm_random(doc.lineLengthBytes(line) + 1);

    seq.append(std::make_unique<TDC_InsertText>(
      TextMCoord(line, byteIndex), randomStringNoNL(20)));
  }

  else if (c.check(20)) {
    // Delete text.
    int line = sm_random(doc.numLines());
    int len = doc.lineLengthBytes(line);

    int startByteIndex = sm_random(len+1);
    int endByteIndex = sm_random(len+1);
    swapIfGreaterThan(startByteIndex, endByteIndex);

    seq.append(std::make_unique<TDC_DeleteText>(
      TextMCoord(line, startByteIndex), endByteIndex - startByteIndex));
  }

  else {
    xfailure("impossible");
  }

  return seq;
}


// EOF
