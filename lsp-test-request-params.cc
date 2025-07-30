// lsp-test-request-params.cc
// Code for `lsp-test-request-params.h`.

// See license.txt for copyright and terms of use.

#include "lsp-test-request-params.h"   // this module

#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-span-ops.h"        // smbase::Span
#include "smbase/string-util.h"        // parseDecimalInt_noSign

#include <cstdlib>                     // std::exit
#include <string_view>                 // std::string_view (for parseDecimalInt_noSign)

using namespace smbase;


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
  Span<std::string const> argv)
{
  if (argv.size() <= 1) {
    // Default query parameters, used when run without arguments.
    return LSPTestRequestParams("eclf.h", 9, 5, false /*useReal*/);
  }

  else {
    if (argv.size() != 4) {
      std::cerr << "Usage: " << argv[0] << " <file> <line> <col>\n";
      std::exit(2);
    }

    std::string fname = argv[1];

    // The LSP protocol uses 0-based lines and columns, but I normally
    // work with 1-based coordinates, so convert those here.  (I do not
    // convert back in the output, however; the responses are just shown
    // as they were sent.)
    int line = parseDecimalInt_noSign(argv[2])-1;
    int col = parseDecimalInt_noSign(argv[3])-1;

    return LSPTestRequestParams(fname, line, col, true /*useReal*/);
  }
}


// EOF
