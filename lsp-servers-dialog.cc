// lsp-servers-dialog.cc
// Code for `lsp-servers-dialog` module.

#include "lsp-servers-dialog.h"        // this module

#include "lsp-client-manager.h"        // LSPClientManager

#include "smqtutil/qtguiutil.h"        // removeWindowContextHelpButton
#include "smqtutil/qtutil.h"           // SET_QOBJECT_NAME
#include "smqtutil/sm-table-widget.h"  // SMTableWidget

#include "smbase/chained-cond.h"       // smbase::cc::z_le_lt
#include "smbase/exc.h"                // GENERIC_CATCH_BEGIN
#include "smbase/refct-serf.h"         // SerfRefCount, NNRCSerf
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // IMEMBFP
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // replaceAllMultiple
#include "smbase/stringb.h"            // stringb

#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace smbase;


INIT_TRACE("lsp-servers-dialog");


LSPServersDialog::~LSPServersDialog()
{
  TRACE1("dtor");
  QObject::disconnect(m_lspClientManager, nullptr, this, nullptr);
}


LSPServersDialog::LSPServersDialog(
  NNRCSerf<LSPClientManager> lspClientManager)
:
  QDialog(),
  SerfRefCount(),
  IMEMBFP(lspClientManager)
{
  TRACE1("ctor");
  setObjectName("LSPServersDialog");
  setWindowTitle("Language Server Protocol Servers");
  resize(800, 600);
  setModal(false);
  removeWindowContextHelpButton(this);

  QVBoxLayout *vbox = new QVBoxLayout(this);
  setLayout(vbox);

  // Table.
  {
    m_table = new SMTableWidget(this);
    SET_QOBJECT_NAME(m_table);

    m_table->configureAsListView();
    m_table->setColumnsFillWidth(true);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);

    std::vector<SMTableWidget::ColumnInfo> columnInfo = {
      // name           init min max prio
      { "Host",          100, 50,  {}, 0 },
      { "Directory",     400, 50,  {}, 1 },
      { "Doc Type",      100, 50, 150, 0 },
      { "Open Docs",     100, 50, 150, 0 },
      { "State",         200, 50, 300, 0 },
    };
    m_table->setColumnInfo(columnInfo);

    // Right-align the "Open Docs" title since the contents are also
    // right-aligned.
    m_table->horizontalHeaderItem(3)->
      setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    vbox->addWidget(m_table);
  }

  // Buttons.
  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    #define ADD_BUTTON(label, reaction)                         \
      QPushButton *reaction##Button = new QPushButton(label);   \
      hbox->addWidget(reaction##Button);                        \
      SET_QOBJECT_NAME(reaction##Button);                       \
      QObject::connect(reaction##Button, &QPushButton::clicked, \
                       this, &LSPServersDialog::reaction);

    ADD_BUTTON("&Help", showHelp);
    ADD_BUTTON("St&art server", startSelectedServer);
    ADD_BUTTON("Sto&p server", stopSelectedServer);
    ADD_BUTTON("&Open LSP stderr in editor", openSelectedStderr);

    hbox->addStretch(1);

    ADD_BUTTON("Close", hide);

    #undef ADD_BUTTON
  }

  QObject::connect(
    m_lspClientManager, &LSPClientManager::signal_changedNumClients,
    this, &LSPServersDialog::repopulateTable);
  QObject::connect(
    m_lspClientManager, &LSPClientManager::signal_changedProtocolState,
    this, &LSPServersDialog::repopulateTable);
  QObject::connect(
    m_lspClientManager, &LSPClientManager::signal_changedNumOpenFiles,
    this, &LSPServersDialog::repopulateTable);

  repopulateTable();
}


void LSPServersDialog::repopulateTable() noexcept
{
  GENERIC_CATCH_BEGIN

  int const numRows = m_lspClientManager->numClients();
  m_table->setRowCount(numRows);

  TRACE1("repopulateTable: numRows=" << numRows);

  for (int row=0; row < numRows; ++row) {
    NNRCSerf<ScopedLSPClient const> client =
      m_lspClientManager->getClientAtIndex(row);
    LSPClientScope const &scope = client->scope();

    // Next column index to use.
    int col = 0;

    // Set the next cell to `text`.
    auto setNextCell = [this, row, &col](
      std::string text,
      bool alignRight = false) -> void
    {
      QTableWidgetItem *item = new QTableWidgetItem(toQString(text));
      if (alignRight) {
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
      }
      m_table->setItem(row, col++, item);
    };

    setNextCell(scope.hostString());
    setNextCell(scope.m_directory.value_or("N/A"));
    setNextCell(scope.languageName());
    setNextCell(stringb(client->client().numOpenFiles()),
                true /*alignRight*/);
    setNextCell(toString(client->client().getProtocolState()));
  }

  GENERIC_CATCH_END
}


static char const *helpText =
R"(This table shows the set of active LSP server connection objects.

A server connection object has a "scope", which specifies the set of
editor documents to which it applies.  A scope consists of a host, an
optional directory, and a document type (language).

A server connection object also can have a running LSP server process.
The buttons in this dialog can be used to stop and start that process.

The "Open LSP stderr in editor" button opens, as an editor document, the
file to which any data written by the server to its stderr is saved.
Servers can write various diagnostics to their stderr, which can be
useful for troubleshooting.
)";


// Within `src`, replace each single newline with a space, preserving
// pairs of consecutive newlines.
static std::string combineParagraphs(std::string const &src)
{
  return replaceAllMultiple(src, {
    { "\n\n", "\n\n" },
    { "\n", " " }
  });
}


void LSPServersDialog::showHelp() noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE1("showHelp");
  QMessageBox::information(this, "LSP Servers Dialog Help",
    toQString(combineParagraphs(helpText)));

  GENERIC_CATCH_END
}


RCSerfOpt<ScopedLSPClient const>
LSPServersDialog::getSelectedServer()
{
  int const numRows = m_lspClientManager->numClients();
  int const row = m_table->currentRow();

  if (cc::z_le_lt(row, numRows)) {
    NNRCSerf<ScopedLSPClient const> ret =
      m_lspClientManager->getClientAtIndex(row);

    TRACE1("getSelectedServer: selected scope: " << ret->scope());

    return ret;
  }
  else {
    TRACE1("getSelectedServer: nothing selected");
    QMessageBox::information(this, "Nothing is selected",
      "Nothing is selected.  Select a table row first.");
    return {};
  }
}


void LSPServersDialog::startSelectedServer() noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE1("startSelectedServer");

  if (RCSerfOpt<ScopedLSPClient const> client =
        getSelectedServer()) {
    FailReasonOpt reason =
      m_lspClientManager->startServerForScope(client->scope());

    repopulateTable();

    if (reason) {
      QMessageBox::warning(this, "Error", toQString(*reason));
    }
  }

  GENERIC_CATCH_END
}


void LSPServersDialog::stopSelectedServer() noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE1("stopSelectedServer");

  if (RCSerfOpt<ScopedLSPClient const> client =
        getSelectedServer()) {
    // Unlike starting, stopping always returns a string.
    std::string result =
      m_lspClientManager->stopServerForScope(client->scope());

    repopulateTable();

    QMessageBox::information(this, "Result", toQString(result));
  }

  GENERIC_CATCH_END
}


void LSPServersDialog::openSelectedStderr() noexcept
{
  GENERIC_CATCH_BEGIN

  TRACE1("openSelectedStderr");

  if (RCSerfOpt<ScopedLSPClient const> client =
        getSelectedServer()) {
    if (std::optional<std::string> logFname =
          client->client().lspStderrLogFname()) {
      TRACE1("startSelectedServer: logFname: " << *logFname);
      Q_EMIT signal_openFileInEditor(*logFname);
    }
    else {
      QMessageBox::information(this, "No Stderr File",
        "The selected LSP server does not have an associated "
        "stderr log file.  (Normally it does, but setting the "
        "EXCLUSIVE_FILE_MAX_SUFFIX envvar to 0 disables it.)");
    }
  }

  GENERIC_CATCH_END
}


// EOF
