// connections-dialog.h
// ConnectionsDialog class.

#ifndef EDITOR_CONNECTIONS_DIALOG_H
#define EDITOR_CONNECTIONS_DIALOG_H

#include "host-name.h"                 // HostName
#include "vfs-connections-fwd.h"       // VFS_Connections

#include "smqtutil/sm-table-widget.h"  // SMTableWidget

#include "smbase/refct-serf.h"         // RCSerf

#include <QDialog>

#include <set>                         // std::set
#include <vector>                      // std::vector

class QPushButton;


// Modeless dialog to show and manipulate the set of VFS connections.
class ConnectionsDialog : public QDialog {
  Q_OBJECT

private:     // instance data
  // Connections being managed.
  RCSerf<VFS_Connections> m_vfsConnections;

  // Widget showing the list.
  SMTableWidget *m_tableWidget;

  // Buttons.
  QPushButton *m_refreshButton;
  QPushButton *m_connectButton;
  QPushButton *m_restartButton;
  QPushButton *m_disconnectButton;
  QPushButton *m_closeButton;

  // Sequence of host names shown in the table.
  std::vector<HostName> m_hostNameList;

private:     // methods
  // Clear and then populate 'm_tableWidget'.
  void repopulateTable();

protected:   // methods
  virtual void showEvent(QShowEvent *event) noexcept override;
  virtual void keyPressEvent(QKeyEvent *k) noexcept override;

public:      // methods
  ConnectionsDialog(VFS_Connections *vfsConnections);
  ~ConnectionsDialog() noexcept;

  // Show an error dialog box.
  void complain(string msg);

  // Get the set of host names corresponding to the set of selected
  // rows.
  std::set<HostName> selectedHostNames() const;

private Q_SLOTS:
  // Handlers for connection state changes.
  void on_vfsConnected(HostName hostName) noexcept;
  void on_vfsFailed(HostName hostName, string reason) noexcept;

  // Handlers for button presses.
  void on_refreshPressed() noexcept;
  void on_connectPressed() noexcept;
  void on_restartPressed() noexcept;
  void on_disconnectPressed() noexcept;
  void on_closePressed() noexcept;
};


#endif // EDITOR_CONNECTIONS_DIALOG_H
