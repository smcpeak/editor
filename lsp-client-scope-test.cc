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
  {
    LSPClientScope s = LSPClientScope::localCPP();
    EXPECT_EQ(s.hostString(), "local");
    EXPECT_EQ(s.hasDirectory(), false);
    EXPECT_EQ(s.languageName(), "C++");
    EXPECT_EQ(s.description(),
      "C++ files on local host");
    EXPECT_EQ(s.semiUniqueIDString(),
      "local-cpp");
  }

  {
    LSPClientScope s =
      LSPClientScope(HostName::asSSH("some-machine"),
                     "/home/user/project/",
                     DocumentType::DT_PYTHON);
    EXPECT_EQ(s.hostString(), "ssh:some-machine");
    EXPECT_EQ(s.hasDirectory(), true);
    EXPECT_EQ(s.directory(), "/home/user/project/");
    EXPECT_EQ(s.directoryFinalName(), "project");
    EXPECT_EQ(s.languageName(), "Python");
    EXPECT_EQ(s.description(),
      "Python files on ssh:some-machine host "
      "and in directory \"/home/user/project/\"");
    EXPECT_EQ(s.semiUniqueIDString(),
      "ssh-some-machine-project-python");
  }
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_lsp_client_scope(CmdlineArgsSpan args)
{
  test_basics();
}


// EOF
