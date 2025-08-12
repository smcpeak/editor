// connections-dialog-test.cc
// Tests for `connections-dialog` module.

#include "connections-dialog.h"        // module under test

#include "vfs-connections.h"           // VFS_Connections

#include <QApplication>


int connections_dialog_test(QApplication &app)
{
  // The normal initial state is with one connection to the local
  // machine.
  VFS_Connections connections;
  connections.connectLocal();

  ConnectionsDialog dlg(&connections);

  QObject::connect(&dlg, &QObject::destroyed, &app, &QApplication::quit);

  dlg.show();
  return app.exec();
}


// EOF
