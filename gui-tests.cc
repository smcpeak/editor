// gui-tests.cc
// Driver program for non-automated GUI tests.

#include "smbase/string-util.h"        // doubleQuote

#include "editor-proxy-style.h"        // EditorProxyStyle
#include "pixmaps.h"                   // Pixmaps

#include <QApplication>
#include <QFont>

#include <exception>                   // std::exception
#include <cstring>                     // std::strcmp
#include <iostream>                    // std::cerr
#include <map>                         // std::map
#include <string>                      // std::string


int innerMain(int argc, char **argv)
{
  QApplication app(argc, argv);

  // This loads the pixmaps and sets `g_editorPixmaps`.
  Pixmaps pixmaps;

  // Override styles.  `app` takes ownership of the style object.
  app.setStyle(new EditorProxyStyle);

  // Map from module name to its test function.
  typedef int (*TestFunc)(QApplication &app);
  std::map<std::string, TestFunc> testFuncs;

  // Macro to declare the entry point and add it to the map.
  #define TEST_MODULE(module)                                             \
    extern int module##_test(QApplication &app);                          \
    testFuncs.insert({std::string(#module), &module##_test}) /* user ; */

  TEST_MODULE(connections_dialog);
  TEST_MODULE(diagnostic_details_dialog);

  #undef TEST_MODULE

  if (argc < 2) {
    std::cerr <<
      "usage: " << argv[0] << " <module>\n"
      "\n"
      "modules:\n";
    for (auto const &kv : testFuncs) {
      std::cerr << "  " << kv.first << "\n";
    }
    return 2;
  }

  // Use a larger (12-point) font.
  QFont fontSpec = QApplication::font();
  fontSpec.setPointSize(12);
  QApplication::setFont(fontSpec);

  // Process command line.
  for (int i=1; i < argc; ++i) {
    char const *arg = argv[i];

    // If `arg` is the name of a module, dispatch to its function.
    for (auto const &kv : testFuncs) {
      std::string const &name = kv.first;
      TestFunc func = kv.second;

      if (name == arg) {
        return (*func)(app);
      }
    }

    std::cerr << "Unknown gui test module: "
              << doubleQuote(arg) << "\n";
    return 2;
  }

  return 0;
}


int main(int argc, char **argv)
{
  try {
    return innerMain(argc, argv);
  }
  catch (std::exception &x) {
    std::cerr << x.what() << "\n";
    return 2;
  }
}


// EOF
