// gui-tests.cc
// Driver program for non-automated GUI tests.

#include "smbase/string-util.h"        // doubleQuote

#include <QApplication>

#include <cstring>                     // std::strcmp
#include <iostream>                    // std::cerr


int diagnostic_details_dialog_test(QApplication &app);


int main(int argc, char **argv)
{
  QApplication app(argc, argv);

  if (argc < 2) {
    std::cerr <<
      "usage: " << argv[0] << " <module>\n"
      "\n"
      "modules:\n"
      "  diagnostic_details_dialog\n";
    return 2;
  }

  for (int i=1; i < argc; ++i) {
    char const *arg = argv[i];

    if (0==std::strcmp(arg, "diagnostic_details_dialog")) {
      return diagnostic_details_dialog_test(app);
    }

    else {
      std::cerr << "Unknown gui test module: "
                << doubleQuote(arg) << "\n";
      return 2;
    }
  }

  return 0;
}


// EOF
