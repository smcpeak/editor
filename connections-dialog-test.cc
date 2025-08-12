// connections-dialog-test.cc
// Tests for `connections-dialog` module.

#include "connections-dialog.h"        // module under test

#include "vfs-connections.h"           // VFS_Connections

#include <QApplication>


int connections_dialog_test(QApplication &app)
{
  VFS_Connections connections;
  ConnectionsDialog dlg(&connections);

  QObject::connect(&dlg, &QObject::destroyed, &app, &QApplication::quit);

  dlg.show();
  return app.exec();
}


// EOF
