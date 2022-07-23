// connections-dialog.cc
// Code for connections-dialog.h.

#include "connections-dialog.h"        // this module

// editor
#include "pixmaps.h"                   // g_editorPixmaps
#include "vfs-connections.h"           // VFS_Connections

// smqtutil
#include "qtguiutil.h"                 // keysString
#include "qtutil.h"                    // SET_QOBJECT_NAME, toQString

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// qt
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>


// Height of each row in pixels.
int const ROW_HEIGHT = 20;

// Initial dialog dimensions in pixels.
int const INIT_DIALOG_WIDTH = 500;
int const INIT_DIALOG_HEIGHT = 400;


MyTableWidget::ColumnInitInfo const
  ConnectionsDialog::s_columnInitInfo[NUM_TABLE_COLUMNS] =
{
  {
    "Host name",
    350,
  },
  {
    "Status",
    100,
  },
};


// Iterate over TableColumn.
#define FOREACH_TABLE_COLUMN(tc)            \
  for (TableColumn tc = (TableColumn)0;     \
       tc < NUM_TABLE_COLUMNS;              \
       tc = (TableColumn)(tc+1))


ConnectionsDialog::ConnectionsDialog(VFS_Connections *vfsConnections)
  : QDialog(nullptr /*parent*/,
            Qt::WindowTitleHint |                // Window has title bar.
            Qt::WindowSystemMenuHint |           // Window system menu (move, etc.)
            Qt::WindowCloseButtonHint),          // Has a close button.
    m_vfsConnections(vfsConnections),
    m_tableWidget(nullptr),
    m_refreshButton(nullptr),
    m_connectButton(nullptr),
    m_restartButton(nullptr),
    m_disconnectButton(nullptr),
    m_closeButton(nullptr)
{
  setObjectName("ConnectionsDialog");
  setWindowTitle("Editor Connections");
  setWindowIcon(g_editorPixmaps->connectionsIcon);

  QVBoxLayout *outerVBox = new QVBoxLayout();
  this->setLayout(outerVBox);

  m_tableWidget = new MyTableWidget();
  outerVBox->addWidget(m_tableWidget);
  SET_QOBJECT_NAME(m_tableWidget);

  m_tableWidget->configureAsListView();
  m_tableWidget->initializeColumns(s_columnInitInfo, NUM_TABLE_COLUMNS);

  {
    QHBoxLayout *buttonsHBox = new QHBoxLayout();
    outerVBox->addLayout(buttonsHBox);

    #define ADD_BUTTON(name, label)                                   \
      m_##name##Button = new QPushButton(label);                      \
      buttonsHBox->addWidget(m_##name##Button);                       \
      SET_QOBJECT_NAME(m_##name##Button);                             \
      QObject::connect(m_##name##Button, &QPushButton::clicked,       \
                       this, &ConnectionsDialog::on_##name##Pressed);

    ADD_BUTTON(refresh, "Refresh (F5)");
    ADD_BUTTON(connect, "&Connect");
    ADD_BUTTON(restart, "&Restart");
    ADD_BUTTON(disconnect, "&Disconnect");

    buttonsHBox->addStretch(1);

    ADD_BUTTON(close, "Close (Esc)");

    #undef ADD_BUTTON
  }

  this->resize(INIT_DIALOG_WIDTH, INIT_DIALOG_HEIGHT);
}


ConnectionsDialog::~ConnectionsDialog() noexcept
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_tableWidget, nullptr, this, nullptr);
  QObject::disconnect(m_disconnectButton, nullptr, this, nullptr);
}


void ConnectionsDialog::repopulateTable()
{
  std::vector<HostName> hostNames = m_vfsConnections->getHostNames();

  m_tableWidget->clearContents();
  m_tableWidget->setRowCount(hostNames.size());

  // Populate the rows.
  for (int r=0; r < (int)hostNames.size(); r++) {
    HostName const &hostName = hostNames.at(r);

    bool connecting = m_vfsConnections->isConnecting(hostName);
    bool ready      = m_vfsConnections->isReady(hostName);
    bool wasLost    = m_vfsConnections->connectionWasLost(hostName);

    string status = connecting? "Connecting" :
                    ready?      "Ready" :
                    wasLost?    "Connection Lost" :
                                "Unknown";

    // Remove the row label.  (The default, a NULL item, renders as a
    // row number, which isn't useful here.)
    m_tableWidget->setVerticalHeaderItem(r, new QTableWidgetItem(""));

    // Flags for the items.  The point is to omit Qt::ItemIsEditable.
    Qt::ItemFlags itemFlags =
      Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Host name.
    {
      QTableWidgetItem *item = new QTableWidgetItem(
        toQString(hostName.toString()));
      item->setFlags(itemFlags);
      m_tableWidget->setItem(r, TC_HOST_NAME, item);
    }

    // Status.
    {
      QTableWidgetItem *item = new QTableWidgetItem(
        toQString(status));
      item->setFlags(itemFlags);
      m_tableWidget->setItem(r, TC_STATUS, item);
    }

    // Apparently I have to set every row's height manually.  QTreeView
    // has a 'uniformRowHeights' property, but QListView does not.
    m_tableWidget->setRowHeight(r, ROW_HEIGHT);
  }
}


void ConnectionsDialog::showEvent(QShowEvent *event) noexcept
{
  TRACE("ConnectionsDialog", "showEvent");

  GENERIC_CATCH_BEGIN

  // When the dialog is shown, refresh its contents and select the first
  // cell.
  repopulateTable();
  m_tableWidget->setCurrentCell(0, 0);
  m_tableWidget->setFocus();

  // I don't think this does anything.
  QDialog::showEvent(event);

  GENERIC_CATCH_END
}


void ConnectionsDialog::keyPressEvent(QKeyEvent *k) noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE("ConnectionsDialog", "keyPressEvent: key=" << keysString(*k));

  if (k->modifiers() == Qt::NoModifier) {
    switch (k->key()) {
      case Qt::Key_F5:
        repopulateTable();
        break;

      default:
        break;
    }
  }

  QDialog::keyPressEvent(k);

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_connected(HostName hostName) noexcept
{
  TRACE("ConnectionsDialog", "on_connected: host=" << hostName);

  GENERIC_CATCH_BEGIN

  // For the moment, just crudely repopulate.
  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_vfsConnectionLost(HostName hostName, string reason) noexcept
{
  TRACE("ConnectionsDialog", "on_vfsConnectionLost: host=" << hostName);

  GENERIC_CATCH_BEGIN

  // For the moment, just crudely repopulate.
  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_refreshPressed() noexcept
{
  TRACE("ConnectionsDialog", "on_refreshPressed");

  GENERIC_CATCH_BEGIN

  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_connectPressed() noexcept
{
  TRACE("ConnectionsDialog", "on_connectPressed");

  GENERIC_CATCH_BEGIN

  QMessageBox box;
  box.setText("TODO: connect");
  box.exec();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_restartPressed() noexcept
{
  TRACE("ConnectionsDialog", "on_restartPressed");

  GENERIC_CATCH_BEGIN

  QMessageBox box;
  box.setText("TODO: restart");
  box.exec();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_disconnectPressed() noexcept
{
  TRACE("ConnectionsDialog", "on_disconnectPressed");

  GENERIC_CATCH_BEGIN

  QMessageBox box;
  box.setText("TODO: disconnect");
  box.exec();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_closePressed() noexcept
{
  TRACE("ConnectionsDialog", "on_closePressed");

  GENERIC_CATCH_BEGIN

  hide();

  GENERIC_CATCH_END
}


// EOF
