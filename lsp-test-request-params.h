// lsp-test-request-params.h
// `LSPTestRequestParams` class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_TEST_REQUEST_PARAMS_H
#define EDITOR_LSP_TEST_REQUEST_PARAMS_H

#include "smbase/sm-span-fwd.h"        // smbase::Span

#include <string>                      // std::string


// Parameters for the requests exercised by the LSP test programs.  They
// correspond to command line arguments.
class LSPTestRequestParams {
public:      // data
  // Name of the source file to get info about.
  std::string m_fname;

  // 0-based line and column of location of interest.
  int m_line;
  int m_col;

  // True to use the real `clangd`, false to use a stand-in script.
  bool m_useRealClangd;

  // Contents to send to the server for this file.
  std::string m_fileContents;

public:      // methods
  ~LSPTestRequestParams();

  // Set all fields.  The contents are read from disk, throwing an
  // exception on failure.
  LSPTestRequestParams(
    std::string const &fname,
    int line,
    int col,
    bool useRealClangd);

  // Return parameters as specified in `argv`.
  static LSPTestRequestParams getFromCmdLine(
    smbase::Span<std::string const> argv);
};


#endif // EDITOR_LSP_TEST_REQUEST_PARAMS_H
