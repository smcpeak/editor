// lsp-manager-test.h
// `LSPManagerTester` class declaration for `lsp-manager-test.cpp`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_MANAGER_TEST_H
#define EDITOR_LSP_MANAGER_TEST_H

#include "lsp-manager.h"               // LSPManager
#include "lsp-test-request-params.h"   // LSPTestRequestParams

#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QObject>


// Test harness for `LSPManager`.  Also serves as the recipients for its
// signals.
class LSPManagerTester : public QObject {
  Q_OBJECT;

public:      // data
  // The manager we are testing.
  LSPManager m_lspManager;

  // Request details from the command line.
  LSPTestRequestParams m_params;

public:      // methods
  ~LSPManagerTester();

  LSPManagerTester(LSPTestRequestParams const &params);

  void testSynchronously();
};


#endif // EDITOR_LSP_MANAGER_TEST_H
