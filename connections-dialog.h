// connections-dialog.h
// ConnectionsDialog class.

#ifndef EDITOR_CONNECTIONS_DIALOG_H
#define EDITOR_CONNECTIONS_DIALOG_H

// editor
#include "host-name.h"                 // HostName
#include "my-table-widget.h"           // MyTableWidget
#include "vfs-connections-fwd.h"       // VFS_Connections

// qt
#include <QDialog>

// libc++
#include <set>                         // std::set
#include <vector>                      // std::vector

class QPushButton;


// Modeless dialog to show and manipulate the set of VFS connections.
class ConnectionsDialog : public QDialog {
  Q_OBJECT

public:      // types
  // The columns of this table.
  enum TableColumn {
    TC_HOST_NAME,
    TC_STATUS,

    NUM_TABLE_COLUMNS
  };

private:     // class data
  // Information about each column.
  static MyTableWidget::ColumnInitInfo const
    s_columnInitInfo[NUM_TABLE_COLUMNS];

private:     // instance data
  // Connections being managed.
  VFS_Connections *m_vfsConnections;

  // Widget showing the list.
  MyTableWidget *m_tableWidget;

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
  void on_connected(HostName hostName) noexcept;
  void on_failed(HostName hostName, string reason) noexcept;

  // Handlers for button presses.
  void on_refreshPressed() noexcept;
  void on_connectPressed() noexcept;
  void on_restartPressed() noexcept;
  void on_disconnectPressed() noexcept;
  void on_closePressed() noexcept;
};


#endif // EDITOR_CONNECTIONS_DIALOG_H
