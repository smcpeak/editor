// diagnostic-details-dialog-test.cc
// Non-automated test harness for `diagnostic-details-dialog` module.

#include "diagnostic-details-dialog.h" // module under test

#include <QApplication>
#include <QFont>
#include <QMessageBox>

#include <utility>                     // std::move

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  // Use a larger (12-point) font.
  {
    QFont fontSpec = QApplication::font();
    fontSpec.setPointSize(12);
    QApplication::setFont(fontSpec);
  }

  typedef DiagnosticDetailsDialog::Element Element;
  QVector<Element> diagnostics;
  diagnostics.reserve(10);

  for (int i = 0; i < 10; ++i) {
    Element de;
    de.m_dir = QString("/long/path/to/source/directory/number/%1/").arg(i);
    de.m_file = QString("file%1.cpp").arg(i);
    de.m_line = i * 10 + 1;
    if (i == 5) {
      de.m_message = QString("This is a very long diagnostic message. ").repeated(40);
    } else {
      de.m_message = QString("Message for element %1.").arg(i);
    }
    diagnostics.append(de);
  }

  DiagnosticDetailsDialog *dlg = new DiagnosticDetailsDialog;
  dlg->setAttribute(Qt::WA_DeleteOnClose);  // Clean up when closed
  dlg->setDiagnostics(std::move(diagnostics));

  QObject::connect(dlg, &DiagnosticDetailsDialog::signal_jumpToLocation,
                   [dlg](const QString &path, int line) {
    QMessageBox::information(dlg, "Jump To",
      QString("Jump to:\n%1\nLine: %2").arg(path).arg(line));
  });

  QObject::connect(dlg, &QObject::destroyed, &app, &QApplication::quit);

  dlg->show();
  return app.exec();
}


// EOF
