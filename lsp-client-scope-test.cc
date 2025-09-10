// lsp-client-scope-test.cc
// Tests for `lsp-client-scope` module.

#include "unit-tests.h"                // decl for my entry point
#include "lsp-client-scope.h"          // module under test

#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_basics()
{
  EXPECT_EQ(
    LSPClientScope::localCPP().description(),
    "C++ files on local host");

  EXPECT_EQ(
    LSPClientScope(HostName::asSSH("some-machine"),
                   DocumentType::DT_OCAML).description(),
    "OCaml files on ssh:some-machine host");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_lsp_client_scope(CmdlineArgsSpan args)
{
  test_basics();
}


// EOF
