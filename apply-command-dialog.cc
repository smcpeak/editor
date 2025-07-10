// apply-command-dialog.cc
// Code for `apply-command-dialog.h`.

#include "apply-command-dialog.h"      // this module

#include "editor-global.h"             // EditorGlobal
#include "editor-settings.h"           // EditorSettings
#include "editor-widget.h"             // EditorWidget

#include "smqtutil/qtutil.h"           // toQString, toString(QSize)

#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/gdvalue.h"            // gdv::toGDValue
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>


using namespace gdv;


INIT_TRACE("apply-command-dialog");


void ApplyCommandDialog::copyToNew() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QListWidgetItem *item = m_prevCommandsListWidget->currentItem();
  if (item) {
    m_newCommandLineEdit->setText(item->text());
    TRACE1("copied text: " << toString(item->text()));
  }
  else {
    TRACE1("no selected item to copy");
  }

  GENERIC_CATCH_END
}


void ApplyCommandDialog::deleteSelected() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  int row = m_prevCommandsListWidget->currentRow();
  if (row < 0) {
    TRACE1("delete: no selected row");
    return;
  }
  else {
    TRACE1("delete: current row: " << row);
  }

  QListWidgetItem *item = m_prevCommandsListWidget->takeItem(row);
  std::string cmd = toString(item->text());
  if (!m_editorWidget->editorGlobal()->
         settings_removeHistoryCommand(this, m_whichFunction, cmd)) {
    DEV_WARNING("delete: non-existent command: " << doubleQuote(cmd));
  }
  else {
    TRACE1("deleted command: " << doubleQuote(cmd));
  }

  GENERIC_CATCH_END
}


void ApplyCommandDialog::clearNewCommand() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_newCommandLineEdit->clear();
  TRACE1("cleared \"new command\" box");

  GENERIC_CATCH_END
}


ApplyCommandDialog::ApplyCommandDialog(
  EditorWidget *editorWidget,
  EditorCommandLineFunction whichFunction)

  : ModalDialog(editorWidget),
    m_editorWidget(editorWidget),
    m_whichFunction(whichFunction)
{
  TRACE1("creating ApplyCommandDialog, whichFunction=" <<
         toGDValue(m_whichFunction));

  if (m_whichFunction == ECLF_APPLY) {
    setWindowTitle("Apply Command");
    setObjectName("ApplyCommandDialog");
  }
  else {
    setWindowTitle("Run Command");
    setObjectName("RunCommandDialog");
  }

  // History to use to populate the dialog.
  CommandLineHistory const &history =
    m_editorWidget->editorGlobal()->getSettings().
      getCommandHistoryC(m_whichFunction);

  QVBoxLayout *vbox = new QVBoxLayout(this);

  // Location label.
  {
    HostName hostName =
      m_editorWidget->getDocumentDirectoryHarn().hostName();

    QString dir = toQString(m_editorWidget->getDocumentDirectory());

    if (hostName.isLocal()) {
      m_pwdLabel = new QLabel(
        tr("Command to run in %1.")
          .arg(dir));
    }
    else {
      QString host = toQString(hostName.toString());
      m_pwdLabel = new QLabel(
        tr("Command to run on %1 in %2.")
          .arg(dir)
          .arg(host));
    }
    SET_QOBJECT_NAME(m_pwdLabel);

    vbox->addWidget(m_pwdLabel);
  }

  m_prevCommandsLabel = new QLabel(tr(
    "Run a &previous command (if \"New\" is empty):"));
  SET_QOBJECT_NAME(m_prevCommandsLabel);
  vbox->addWidget(m_prevCommandsLabel);

  m_prevCommandsListWidget = new QListWidget();
  m_prevCommandsLabel->setBuddy(m_prevCommandsListWidget);
  SET_QOBJECT_NAME(m_prevCommandsListWidget);
  vbox->addWidget(m_prevCommandsListWidget);

  // Populate the list.
  {
    int index = 0;
    int rowToSelect = -1;
    for (std::string const &cmd : history.m_commands) {
      m_prevCommandsListWidget->addItem(toQString(cmd));
      if (cmd == history.m_recent) {
        rowToSelect = index;
      }
      ++index;
    }

    if (rowToSelect >= 0) {
      m_prevCommandsListWidget->setCurrentRow(rowToSelect);
      TRACE1("initially selected row " << rowToSelect);
    }
    else {
      TRACE1("no initially selected row");
    }
  }

  // "Copy" and "Delete" buttons.
  {
    QHBoxLayout *hbox = new QHBoxLayout();
    hbox->addStretch();

    m_copyButton = new QPushButton(tr("&Copy to New"));
    SET_QOBJECT_NAME(m_copyButton);
    QObject::connect(m_copyButton, &QPushButton::clicked,
                     this, &ApplyCommandDialog::copyToNew);
    hbox->addWidget(m_copyButton);

    m_deleteButton = new QPushButton(tr("&Delete"));
    SET_QOBJECT_NAME(m_deleteButton);
    QObject::connect(m_deleteButton, &QPushButton::clicked,
                     this, &ApplyCommandDialog::deleteSelected);
    hbox->addWidget(m_deleteButton);

    vbox->addLayout(hbox);
  }

  m_newCommandLabel = new QLabel(tr("Run a &new command (if not empty)"));
  SET_QOBJECT_NAME(m_newCommandLabel);
  vbox->addWidget(m_newCommandLabel);

  // New command line, and its clear button.
  {
    QHBoxLayout *hbox = new QHBoxLayout();

    m_newCommandLineEdit = new QLineEdit();
    m_newCommandLabel->setBuddy(m_newCommandLineEdit);
    SET_QOBJECT_NAME(m_newCommandLineEdit);
    hbox->addWidget(m_newCommandLineEdit);

    // Use a tool button so it is smaller.
    m_clearNewCommandButton = new QToolButton();
    m_clearNewCommandButton->setText("&X");
    m_clearNewCommandButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    SET_QOBJECT_NAME(m_clearNewCommandButton);

    QObject::connect(m_clearNewCommandButton, &QToolButton::clicked,
                     this, &ApplyCommandDialog::clearNewCommand);
    hbox->addWidget(m_clearNewCommandButton);

    vbox->addLayout(hbox);
  }

  m_enableSubstitutionCheckBox = new QCheckBox(tr(
    "Enable &substitution (see help)"));
  m_enableSubstitutionCheckBox->setChecked(history.m_useSubstitution);
  SET_QOBJECT_NAME(m_enableSubstitutionCheckBox);
  vbox->addWidget(m_enableSubstitutionCheckBox);

  if (m_whichFunction == ECLF_RUN) {
    m_prefixStderrLinesCheckBox = new QCheckBox(tr(
      "Prefix stderr lines &with \"STDERR: \""));
    m_prefixStderrLinesCheckBox->setChecked(history.m_prefixStderrLines);
    SET_QOBJECT_NAME(m_prefixStderrLinesCheckBox);
    vbox->addWidget(m_prefixStderrLinesCheckBox);
  }

  createOkAndCancelHBox(vbox);
  createHelpButton();

  if (m_whichFunction == ECLF_APPLY) {
    m_helpText =
      "This passes the selected text (if any) as the stdin of a new "
      "process started with the given command line in the directory "
      "containing the current file.  The resulting stdout is then "
      "inserted into the current document, replacing whatever was "
      "selected.";
  }
  else {
    m_helpText =
      "This spawns a process with the given command line in the "
      "directory containing the current file, and creates a new "
      "editor document containing its output (or replaces one, if "
      "one already exists with the exact same command line and "
      "directory).";
  }

  m_helpText +=
    "\n\n"
    "If \"Enable substitution\" is checked, then the following "
    "substitutions will be performed on the command line before "
    "executing:\n"
    "\n\n"
    "  - $f: Current document file name, without directory\n";

  if (m_whichFunction == ECLF_RUN) {
    m_helpText +=
      "\n\n"
      "If \"Prefix stderr\" is checked, then the command will be run "
      "with stdout and stderr going to separate streams, and stderr "
      "lines will have \"STDERR: \" prefixed for identification.  "
      "However, this means the precise temporal interleaving between "
      "output and error is lost.";
  }

  // Calculate size based on layout.
  adjustSize();
  TRACE2("size after adjustSize: " << toString(size()));

  // Ensure it is at least 550x450 initially.
  resize(size().expandedTo(QSize(550, 450)));
  TRACE2("size after resize: " << toString(size()));
}


ApplyCommandDialog::~ApplyCommandDialog()
{
  TRACE1("running destructor");

  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_copyButton, nullptr, this, nullptr);
  QObject::disconnect(m_deleteButton, nullptr, this, nullptr);
  QObject::disconnect(m_clearNewCommandButton, nullptr, this, nullptr);
}


QString ApplyCommandDialog::getSpecifiedCommand() const
{
  if (QString cmd = m_newCommandLineEdit->text().trimmed();
      !cmd.isEmpty()) {
    return cmd;
  }

  if (QListWidgetItem *item = m_prevCommandsListWidget->currentItem()) {
    return item->text();
  }

  return {};
}


bool ApplyCommandDialog::isSubstitutionEnabled() const
{
  return m_enableSubstitutionCheckBox->isChecked();
}


bool ApplyCommandDialog::isPrefixStderrEnabled() const
{
  if (m_prefixStderrLinesCheckBox) {
    return m_prefixStderrLinesCheckBox->isChecked();
  }
  else {
    return false;
  }
}


void ApplyCommandDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  QString cmd = getSpecifiedCommand();
  if (!cmd.isEmpty()) {
    TRACE1("accept: specified command: " << doubleQuote(cmd));
    TRACE1("accept: substitution enabled: " <<
           toGDValue(isSubstitutionEnabled()));
    TRACE1("accept: prefix enabled: " <<
           toGDValue(isPrefixStderrEnabled()));
    ModalDialog::accept();
  }
  else {
    QMessageBox::information(this,
      tr("No command to run"),
      tr("There is no command to run; the \"new command\" box is "
         "empty, and the history list has nothing selected."));
  }

  GENERIC_CATCH_END
}


// EOF
