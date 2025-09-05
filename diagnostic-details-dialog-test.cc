// diagnostic-details-dialog-test.cc
// Non-automated test harness for `diagnostic-details-dialog` module.

#include "diagnostic-details-dialog.h" // module under test

#include "diagnostic-element.h"        // DiagnosticElement
#include "line-number.h"               // LineNumber

#include "smqtutil/qstringb.h"         // qstringb

#include "smbase/sm-env.h"             // smbase::envOrEmpty
#include "smbase/string-util.h"        // repeatString
#include "smbase/stringb.h"            // stringb

#include <QApplication>
#include <QMessageBox>

#include <utility>                     // std::move

using namespace smbase;


// Called from gui-tests.cc.
int diagnostic_details_dialog_test(QApplication &app)
{
  QVector<DiagnosticElement> diagnostics;
  diagnostics.reserve(10);

  // Provide a way to cause the file names to be longer in order to
  // test the column's ability to handle that.
  std::string const nameExtension = envOrEmpty("NAME_EXTENSION");

  for (int i = 0; i < 10; ++i) {
    DiagnosticElement de{
      HostAndResourceName::localFile(stringb(
        "/long/path/to/source/directory/number/" << i <<
        "/file" << i << nameExtension << ".cpp")),
      LineNumber(i * 10 + 1).toLineIndex(),
      (i == 5?
         repeatString("This is a very long diagnostic message. ", 40) :
         stringb("Message for element " << i << "."))
    };

    diagnostics.append(de);
  }

  // This is freed by Qt due to `WA_DeleteOnClose`.
  DiagnosticDetailsDialog *dlg = new DiagnosticDetailsDialog;
  dlg->setAttribute(Qt::WA_DeleteOnClose);
  dlg->setDiagnostics(std::move(diagnostics));

  QObject::connect(dlg, &DiagnosticDetailsDialog::signal_jumpToLocation,
                   [dlg](DiagnosticElement const &element) {
    QMessageBox::information(dlg, "Jump To",
      qstringb("Jump to:\n" <<
               element.m_harn << "\n"
               "Line: " << element.m_lineIndex.toLineNumber()));
  });

  QObject::connect(dlg, &QObject::destroyed, &app, &QApplication::quit);

  dlg->show();
  return app.exec();
}


// EOF
