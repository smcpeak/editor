// unit-test.cc
// Driver for editor module unit tests.

#include "unit-tests.h"                // decls of test entry points

#include "smbase/dev-warning.h"        // g_abortUponDevWarning
#include "smbase/exc.h"                // xfatal, smbase::XBase
#include "smbase/sm-span.h"            // smbase::Span
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/str.h"                // streq
#include "smbase/stringb.h"            // stringb

#include <QCoreApplication>

#include <exception>                   // std::exception

using namespace smbase;


// This must be `static`, rather than in an anonymous namespace, because
// of the `extern` declarations inside it.
static void entry(int argc, char **argv)
{
  // Console-only Qt apps use `QCoreApplication`, which does not need
  // any access to a Windowing API.  This is particularly relevant on
  // unix, where X11 may or may not be available.
  QCoreApplication app(argc, argv);

  // Initially empty.
  CmdlineArgsSpan extraArgs;

  char const *testName = NULL;
  if (argc >= 2) {
    testName = argv[1];

    // We only pass arguments if we are only running one test.
    extraArgs = CmdlineArgsSpan(argv+2, argc-2);
  }

  bool ranOne = false;

  // Run the test if it is enabled.
  #define RUN_TEST(name)                                \
    if (testName == NULL || streq(testName, #name)) {   \
      std::cout << "---- " #name " ----" << std::endl;  \
      test_##name(extraArgs);                           \
      /* Flush all output streams so that the output */ \
      /* from different tests cannot get mixed up. */   \
      std::cout.flush();                                \
      std::cerr.flush();                                \
      ranOne = true;                                    \
    }

  RUN_TEST(bufferlinesource);
  RUN_TEST(c_hilite);
  RUN_TEST(doc_type_detect);
  RUN_TEST(editor_fs_server);
  RUN_TEST(hashcomment_hilite);
  RUN_TEST(lsp_client);
  RUN_TEST(lsp_data);
  RUN_TEST(lsp_manager);
  RUN_TEST(makefile_hilite);
  RUN_TEST(named_td);
  RUN_TEST(named_td_list);
  RUN_TEST(nearby_file);
  RUN_TEST(ocaml_hilite);
  RUN_TEST(python_hilite);
  RUN_TEST(textcategory);
  RUN_TEST(uri_util);
  RUN_TEST(vfs_connections);

  #undef RUN_TEST

  if (!ranOne) {
    xfatal(stringb("unrecogized module name: " << testName));
  }
  else if (testName) {
    std::cout << "tests for module " << testName << " PASSED\n";
  }
  else {
    std::cout << "unit tests PASSED\n";
  }
}


int main(int argc, char *argv[])
{
  g_abortUponDevWarning = true;
  try {
    entry(argc, argv);
    return 0;
  }
  catch (XBase &x) {
    cerr << x.what() << endl;
    return 2;
  }
  catch (std::exception &x) {
    // Some of the std exceptions are not very self-explanatory without
    // also seeing the exception type.  This is ugly because `name()`
    // returns a mangled name, so I'd like to avoid ever allowing such
    // exceptions to propagate.
    cerr << typeid(x).name() << ": " << x.what() << endl;
    return 2;
  }
}


// EOF
