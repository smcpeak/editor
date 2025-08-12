// connections-dialog.cc
// Code for connections-dialog.h.

#include "connections-dialog.h"        // this module

// editor
#include "pixmaps.h"                   // g_editorPixmaps
#include "vfs-connections.h"           // VFS_Connections

// smqtutil
#include "smqtutil/qtguiutil.h"        // keysString
#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME, toQString
#include "smqtutil/sm-table-widget.h"  // operator<<(QModelIndex)

// smbase
#include "smbase/container-util.h"     // smbase::contains
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN/END
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.

// qt
#include <QInputDialog>
#include <QKeyEvent>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace smbase;


INIT_TRACE("connections-dialog");


// Initial dialog dimensions in pixels.
static int const INIT_DIALOG_WIDTH = 500;
static int const INIT_DIALOG_HEIGHT = 400;


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

  xassert(g_editorPixmaps);
  setWindowIcon(g_editorPixmaps->connectionsIcon);

  QVBoxLayout *outerVBox = new QVBoxLayout();
  this->setLayout(outerVBox);

  m_tableWidget = new SMTableWidget();
  outerVBox->addWidget(m_tableWidget);
  SET_QOBJECT_NAME(m_tableWidget);

  m_tableWidget->configureAsListView();
  m_tableWidget->setColumnsFillWidth(true);

  std::vector<SMTableWidget::ColumnInfo> columnInfo = {
    //name        init  min           max  prio
    { "Host name", 350, 100, std::nullopt, 1 },
    { "Status",    100,  50, std::nullopt, 0 },
  };
  m_tableWidget->setColumnInfo(columnInfo);

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
    ADD_BUTTON(connect, "&Connect to...");
    ADD_BUTTON(restart, "&Restart");
    ADD_BUTTON(disconnect, "&Disconnect");

    buttonsHBox->addStretch(1);

    ADD_BUTTON(close, "Close (Esc)");

    #undef ADD_BUTTON
  }

  QObject::connect(m_vfsConnections, &VFS_Connections::signal_vfsConnected,
                   this, &ConnectionsDialog::on_vfsConnected);
  QObject::connect(m_vfsConnections, &VFS_Connections::signal_vfsFailed,
                   this, &ConnectionsDialog::on_vfsFailed);

  repopulateTable();
  m_tableWidget->setCurrentCell(0, 0);
  m_tableWidget->setFocus();

  this->resize(INIT_DIALOG_WIDTH, INIT_DIALOG_HEIGHT);
}


ConnectionsDialog::~ConnectionsDialog() noexcept
{
  // See doc/signals-and-dtors.txt.
  disconnectSignalSender(m_refreshButton);
  disconnectSignalSender(m_connectButton);
  disconnectSignalSender(m_restartButton);
  disconnectSignalSender(m_disconnectButton);
  disconnectSignalSender(m_closeButton);
  disconnectSignalSender(m_vfsConnections);
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
    bool failed     = m_vfsConnections->connectionFailed(hostName);

    string status = connecting? "Connecting" :
                    ready?      "Ready" :
                    failed?     "Failed" :
                                "Unknown";

    // Remove the row label.  (The default, a NULL item, renders as a
    // row number, which isn't useful here.)
    m_tableWidget->setVerticalHeaderItem(r, new QTableWidgetItem(""));

    // Flags for the items.  The point is to omit Qt::ItemIsEditable.
    Qt::ItemFlags itemFlags =
      Qt::ItemIsSelectable | Qt::ItemIsEnabled;

    // Column index.
    int c = 0;

    // Host name.
    {
      QTableWidgetItem *item = new QTableWidgetItem(
        toQString(hostName.toString()));
      item->setFlags(itemFlags);
      m_tableWidget->setItem(r, c++, item /*transfer*/);
    }

    // Status.
    {
      QTableWidgetItem *item = new QTableWidgetItem(
        toQString(status));
      item->setFlags(itemFlags);
      m_tableWidget->setItem(r, c++, item /*transfer*/);
    }

    // Apparently I have to set every row's height manually.
    m_tableWidget->setNaturalTextRowHeight(r);
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
    TRACE1("  selRow: " << index);

    if (index.isValid() && !index.parent().isValid()) {
      int r = index.row();
      ret.insert(m_hostNameList.at(r));
    }
  }

  return ret;
}


void ConnectionsDialog::showEvent(QShowEvent *event) noexcept
{
  TRACE1("showEvent");

  GENERIC_CATCH_BEGIN

  // This is a placeholder.  I might remove it.

  // I don't think this does anything.
  QDialog::showEvent(event);

  GENERIC_CATCH_END
}


void ConnectionsDialog::keyPressEvent(QKeyEvent *k) noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE1("keyPressEvent: key=" << keysString(*k));

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


void ConnectionsDialog::on_vfsConnected(HostName hostName) noexcept
{
  TRACE1("on_vfsConnected: host=" << hostName);

  GENERIC_CATCH_BEGIN

  // For the moment, just crudely repopulate.
  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_vfsFailed(HostName hostName, string reason) noexcept
{
  TRACE1("on_vfsFailed: host=" << hostName);

  GENERIC_CATCH_BEGIN

  // For the moment, just crudely repopulate.
  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_refreshPressed() noexcept
{
  TRACE1("on_refreshPressed");

  GENERIC_CATCH_BEGIN

  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_connectPressed() noexcept
{
  TRACE1("on_connectPressed");

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
  TRACE1("on_restartPressed");

  GENERIC_CATCH_BEGIN

  std::set<HostName> hostNames = selectedHostNames();
  for (HostName const &hostName : hostNames) {
    TRACE1("  restart: " << hostName);
    m_vfsConnections->shutdown(hostName);
    m_vfsConnections->connect(hostName);
  }

  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_disconnectPressed() noexcept
{
  TRACE1("on_disconnectPressed");

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
    TRACE1("  disconnect: " << hostName);
    m_vfsConnections->shutdown(hostName);
  }

  repopulateTable();

  GENERIC_CATCH_END
}


void ConnectionsDialog::on_closePressed() noexcept
{
  TRACE1("on_closePressed");

  GENERIC_CATCH_BEGIN

  hide();

  GENERIC_CATCH_END
}


// EOF
