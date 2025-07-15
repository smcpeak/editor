// lsp-test-request-params.cc
// Code for `lsp-test-request-params.h`.

// See license.txt for copyright and terms of use.

#include "lsp-test-request-params.h"   // this module

#include "smbase/sm-file-util.h"       // SMFileUtil

#include <cstdlib>                     // std::exit


LSPTestRequestParams::~LSPTestRequestParams()
{}


LSPTestRequestParams::LSPTestRequestParams(
  std::string const &fname,
  int line,
  int col,
  bool useRealClangd)
  : m_fname(fname),
    m_line(line),
    m_col(col),
    m_useRealClangd(useRealClangd),
    m_fileContents(SMFileUtil().readFileAsString(fname))
{}


/*static*/ LSPTestRequestParams LSPTestRequestParams::getFromCmdLine(
  int argc, char **argv)
{
  if (argc == 1) {
    // Default query parameters, used when run without arguments.
    return LSPTestRequestParams("eclf.h", 9, 5, false /*useReal*/);
  }

  else {
    if (argc != 4) {
      std::cerr << "Usage: " << argv[0] << " <file> <line> <col>\n";
      std::exit(2);
    }

    std::string fname = argv[1];

    // The LSP protocol uses 0-based lines and columns, but I normally
    // work with 1-based coordinates, so convert those here.  (I do not
    // convert back in the output, however; the responses are just shown
    // as they were sent.)
    int line = std::atoi(argv[2])-1;
    int col = std::atoi(argv[3])-1;

    return LSPTestRequestParams(fname, line, col, true /*useReal*/);
  }
}


// EOF
