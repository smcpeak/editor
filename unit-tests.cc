// unit-test.cc
// Driver for editor module unit tests.

#include "unit-tests.h"                // decls of test entry points

#include "smbase/dev-warning.h"        // g_abortUponDevWarning
#include "smbase/exc.h"                // xfatal, smbase::XBase
#include "smbase/sm-span.h"            // smbase::Span
#include "smbase/sm-test.h"            // EXPECT_EQ
#include "smbase/str.h"                // streq
#include "smbase/stringb.h"            // stringb
#include "smbase/trace.h"              // traceAddFromEnvVar

#include <QCoreApplication>

#include <exception>                   // std::exception

using namespace smbase;


// This must be `static`, rather than in an anonymous namespace, because
// of the `extern` declarations inside it.
static void entry(int argc, char **argv)
{
  // Enable old-style tracing in unit tests.
  traceAddFromEnvVar();

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

  /* This list is meant to be in bottom-up topological order so the
     low-level modules get tested first.  Then, tests should ideally be
     in order from fastest to slowest, but I haven't done systematic
     measurements of that.

     The dependencies listed are what I collected using a script:

       https://github.com/smcpeak/scripts/blob/master/analyze-cpp-module-deps.py

     However, they are incomplete because they are only the direct
     dependencies, so if a module does not have any test, there are
     missing edges.

     I don't necessarily intend to maintain them in this form.  It's a
     first cut though.
  */

  // No deps in this repo (except for `command-runner`).
  RUN_TEST(editor_strutil);            // deps: (none)
  RUN_TEST(gap);                       // deps: (none)
  RUN_TEST(recent_items_list);         // deps: (none)
  RUN_TEST(td_line);                   // deps: (none)
  RUN_TEST(textcategory);              // deps: (none)
  RUN_TEST(uri_util);                  // deps: (none)

  // Wrapped integers.
  RUN_TEST(wrapped_integer);
  RUN_TEST(line_difference);           // deps: wrapped-integer
  RUN_TEST(line_count);                // deps: wrapped-integer, line-difference
  RUN_TEST(line_index);                // deps: wrapped-integer
  RUN_TEST(positive_line_count);       // deps: wrapped-integer, line-count, line-difference
  RUN_TEST(byte_count);
  RUN_TEST(byte_index);
  RUN_TEST(td_version_number);         // deps: wrapped-integer
  RUN_TEST(lsp_version_number);        // deps: wrapped-integer, td-version-number

  // Deps only on things that do not have their own tests.
  RUN_TEST(doc_type_detect);           // deps: doc-name
  RUN_TEST(host_file_and_line_opt);
  RUN_TEST(range_text_repl);           // deps: textmcoord

  // SCC: history, td, td-core
  RUN_TEST(td_core);                   // deps: gap-gdvalue, history, line-index, td, td-line, textmcoord
  RUN_TEST(td);                        // deps: history, line-index, range-text-repl, td-core, textmcoord

  RUN_TEST(td_change);                 // deps: line-index, range-text-repl, td-core, textmcoord

  RUN_TEST(textmcoord_map);            // deps: line-index, td-core, textmcoord

  // SCC: justify, td-editor
  RUN_TEST(justify);                   // deps: line-index, td-editor
  RUN_TEST(td_editor);                 // deps: editor-strutil, justify, td, textcategory, textlcoord

  RUN_TEST(bufferlinesource);          // deps: line-index, td-core, td-editor

  RUN_TEST(c_hilite);                  // deps: bufferlinesource, textcategory
  RUN_TEST(hashcomment_hilite);        // deps: bufferlinesource, textcategory
  RUN_TEST(makefile_hilite);           // deps: bufferlinesource, textcategory
  RUN_TEST(ocaml_hilite);              // deps: bufferlinesource, textcategory
  RUN_TEST(python_hilite);             // deps: bufferlinesource, textcategory

  RUN_TEST(editor_fs_server);          // deps: editor-version, vfs-local

  // SCC: lsp-conv, lsp-data, lsp-manager, named-td, td-diagnostics, td-obs-recorder
  RUN_TEST(lsp_conv);                  // deps: lsp-data, lsp-manager, named-td, range-text-repl, td-change, td-change-seq, td-core, td-diagnostics, td-obs-recorder, textmcoord, uri-util
  RUN_TEST(lsp_data);                  // deps: line-index, lsp-conv, named-td, td-diagnostics, uri-util
  RUN_TEST(td_diagnostics);            // deps: line-index, named-td, td-change, td-change-seq, td-core, textmcoord-map
  RUN_TEST(td_obs_recorder);           // deps: named-td, td-change, td-change-seq, td-core, td-diagnostics
  RUN_TEST(named_td);                  // deps: doc-name, hilite, lsp-conv, lsp-data, td, td-diagnostics, td-obs-recorder

  RUN_TEST(named_td_editor);           // deps: doc-name, named-td, td-editor

  RUN_TEST(named_td_list);             // deps: named-td

  RUN_TEST(nearby_file);               // deps: host-and-resource-name

  RUN_TEST(text_search);               // deps: fasttime, line-index, td-core, td-editor

  RUN_TEST(vfs_connections);           // deps: host-name, vfs-msg, vfs-query

  // This depends on `lsp_manager`, but only in a fairly simple way, and
  // this test should be much faster.
  RUN_TEST(lsp_get_code_lines);

  // This is the slowest test, but lsp-manager uses it, so it needs to
  // be before that.
  RUN_TEST(command_runner);            // deps: (none)

  RUN_TEST(lsp_manager);               // deps: command-runner, line-index, json-rpc-client, lsp-conv, lsp-data, lsp-symbol-request-kind, td-core, td-diagnostics, td-obs-recorder, textmcoord, uri-util
  RUN_TEST(json_rpc_client);           // deps: command-runner, uri-util

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
