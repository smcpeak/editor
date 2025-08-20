// td-change-seq.cc
// Code for `td-change-seq` module.

#include "td-change-seq.h"             // this module

#include "td-change.h"                 // TextDocumentChange

#include "smbase/gdvalue-unique-ptr.h" // gdv::toGDValue(std::unique_ptr)
#include "smbase/gdvalue-vector.h"     // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"            // gdv::GDValue

using namespace gdv;


TextDocumentChangeSequence::~TextDocumentChangeSequence()
{}


TextDocumentChangeSequence::TextDocumentChangeSequence()
  : m_seq()
{}


TextDocumentChangeSequence::TextDocumentChangeSequence(
  TextDocumentChangeSequence &&obj)
  : MDMEMB(m_seq)
{}


std::size_t TextDocumentChangeSequence::size() const
{
  return m_seq.size();
}


TextDocumentChangeSequence::operator gdv::GDValue() const
{
  return toGDValue(m_seq);
}


// EOF
