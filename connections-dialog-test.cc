// connections-dialog-test.cc
// Tests for `connections-dialog` module.

#include "connections-dialog.h"        // module under test

#include "rfa-async-multi-client.h"    // RFA_AsyncMultiClient

#include <QApplication>


int connections_dialog_test(QApplication &app)
{
  // The normal initial state is with one connection to the local
  // machine.
  RFA_AsyncMultiClient rfaAMClient;
  rfaAMClient.connectLocal();

  ConnectionsDialog dlg(&rfaAMClient);

  QObject::connect(&dlg, &QObject::destroyed, &app, &QApplication::quit);

  dlg.show();
  return app.exec();
}


// EOF
