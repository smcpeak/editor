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
#include "container-utils.h"           // contains
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "trace.h"                     // TRACE

// qt
#include <QInputDialog>
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
    m_closeButton(nullptr),
    m_hostNameList()
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

  QObject::connect(m_vfsConnections, &VFS_Connections::signal_connected,
                   this, &ConnectionsDialog::on_connected);
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_failed,
                   this, &ConnectionsDialog::on_failed);

  repopulateTable();
  m_tableWidget->setCurrentCell(0, 0);
  m_tableWidget->setFocus();

  this->resize(INIT_DIALOG_WIDTH, INIT_DIALOG_HEIGHT);
}


ConnectionsDialog::~ConnectionsDialog() noexcept
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_refreshButton,    nullptr, this, nullptr);
  QObject::disconnect(m_connectButton,    nullptr, this, nullptr);
  QObject::disconnect(m_restartButton,    nullptr, this, nullptr);
  QObject::disconnect(m_disconnectButton, nullptr, this, nullptr);
  QObject::disconnect(m_closeButton,      nullptr, this, nullptr);
  QObject::disconnect(m_vfsConnections,   nullptr, this, nullptr);
}


void ConnectionsDialog::complain(string msg)
{
  // Use 'about' to not make noise.
  //
  // This uses the icon for the connections dialog as the icon within
  // the error message box, which is a little weird, but not a major
  // problem.
  QMessageBox::about(this, "Error", toQString(msg));
}


void ConnectionsDialog::repopulateTable()
{
  std::vector<HostName> hostNames = m_vfsConnections->getHostNames();

  m_tableWidget->clearContents();
  m_tableWidget->setRowCount(hostNames.size());
  m_hostNameList.clear();

  // Populate the rows.
  for (int r=0; r < (int)hostNames.size(); r++) {
    HostName const &hostName = hostNames.at(r);
    m_hostNameList.push_back(hostName);

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


std::set<HostName> ConnectionsDialog::selectedHostNames() const
{
  std::set<HostName> ret;

  QItemSelectionModel *selectionModel = m_tableWidget->selectionModel();
  QModelIndexList selectedRows = selectionModel->selectedRows();
  for (QModelIndexList::iterator it(selectedRows.begin());
       it != selectedRows.end();
       ++it)
  {
    QModelIndex index = *it;
    TRACE("ConnectionsDialog", "  selRow: " << index);

    if (index.isValid() && !index.parent().isValid()) {
      int r = index.row();
      ret.insert(m_hostNameList.at(r));
    }
  }

  return ret;
}


void ConnectionsDialog::showEvent(QShowEvent *event) noexcept
{
  TRACE("ConnectionsDialog", "showEvent");

  GENERIC_CATCH_BEGIN

  // This is a placeholder.  I might remove it.

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


void ConnectionsDialog::on_failed(HostName hostName, string reason) noexcept
{
  TRACE("ConnectionsDialog", "on_failed: host=" << hostName);

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

  bool ok;
  QString text = QInputDialog::getText(this,
    "Connect",
    "SSH Host Name:",
    QLineEdit::Normal,
    "",
    &ok);

  if (ok) {
    HostName hostName(HostName::asSSH(toString(text)));
    if (m_vfsConnections->isValid(hostName)) {
      complain(stringb(
        "There is already a connection for " << hostName << "."));
    }
    else {
      m_vfsConnections->connect(hostName);
      repopulateTable();
    }
  }

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_restartPressed() noexcept
{
  TRACE("ConnectionsDialog", "on_restartPressed");

  GENERIC_CATCH_BEGIN

  std::set<HostName> hostNames = selectedHostNames();
  for (HostName const &hostName : hostNames) {
    TRACE("ConnectionsDialog", "  restart: " << hostName);
    m_vfsConnections->shutdown(hostName);
    m_vfsConnections->connect(hostName);
  }

  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_disconnectPressed() noexcept
{
  TRACE("ConnectionsDialog", "on_disconnectPressed");

  GENERIC_CATCH_BEGIN

  // Get the set of hosts to disconnect.
  std::set<HostName> hostNamesToDisconnect = selectedHostNames();
  if (contains(hostNamesToDisconnect, HostName::asLocal())) {
    // The main reason for disallowing this is I do not have a way
    // to re-establish it afterward.
    complain("Cannot disconnect the local connection.");

    // Bail out entirely rather than disconnecting a subset.
    return;
  }

  // Disconnect them.
  for (HostName const &hostName : hostNamesToDisconnect) {
    TRACE("ConnectionsDialog", "  disconnect: " << hostName);
    m_vfsConnections->shutdown(hostName);
  }

  repopulateTable();

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
