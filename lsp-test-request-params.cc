// lsp-test-request-params.cc
// Code for `lsp-test-request-params.h`.

// See license.txt for copyright and terms of use.

#include "lsp-test-request-params.h"   // this module

#include "smbase/sm-file-util.h"       // SMFileUtil


LSPTestRequestParams::~LSPTestRequestParams()
{}


LSPTestRequestParams::LSPTestRequestParams(
  std::string const &fname,
  int line,
  int col)
  : m_fname(fname),
    m_line(line),
    m_col(col),
    m_fileContents(SMFileUtil().readFileAsString(fname))
{}


// EOF
