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


// -------------------------- private methods --------------------------
EditorGlobal *ApplyCommandDialog::editorGlobal() const
{
  return m_editorGlobal;
}


void ApplyCommandDialog::populateListWidget(bool initial)
{
  CommandLineHistory const &history = getHistory();

  // If we are just starting the dialog, select the most-recently
  // executed command.  Otherwise (we are repopulating due to the filter
  // being edited), try to keep the same item selected.
  std::string itemToSelect;
  if (initial) {
    itemToSelect = history.m_recent;
  }
  else if (QListWidgetItem *item = m_prevCommandsListWidget->currentItem()) {
    itemToSelect = toString(item->text());
  }
  TRACE2("itemToSelect: " << doubleQuote(itemToSelect));

  // Discard the current list contents.
  m_prevCommandsListWidget->clear();

  // Is a filter active?
  std::string filterString = toString(m_filterLineEdit->text());
  if (!filterString.empty()) {
    TRACE2("filter: " << doubleQuote(filterString));
  }

  // Populate the list.
  int curRowIndex = 0;
  int rowToSelect = -1;
  for (std::string const &cmd : history.m_commands) {
    if (filterString.empty() || hasSubstring(cmd, filterString)) {
      m_prevCommandsListWidget->addItem(toQString(cmd));
      if (cmd == itemToSelect) {
        rowToSelect = curRowIndex;
      }
      ++curRowIndex;
    }
  }

  // Select a row if we found a good one.
  if (rowToSelect >= 0) {
    m_prevCommandsListWidget->setCurrentRow(rowToSelect);
    TRACE2("selected row " << rowToSelect);
  }
  else {
    TRACE2("no selected row");
  }
}


CommandLineHistory const &ApplyCommandDialog::getHistory()
{
  return editorGlobal()->getSettings().
           getCommandHistoryC(m_whichFunction);
}


void ApplyCommandDialog::moveFocusToCommandsList()
{
  TRACE2("moving focus to commands list");

  m_prevCommandsListWidget->setFocus();

  // Select an item if we can.
  if (m_prevCommandsListWidget->count() > 0) {
    if (QListWidgetItem *item = m_prevCommandsListWidget->currentItem()) {
      if (!item->isSelected()) {
        TRACE2("selecting current item");
        item->setSelected(true);
      }
      else {
        TRACE2("current item is already selected");
      }
    }
    else {
      TRACE2("selecting first item");
      m_prevCommandsListWidget->setCurrentRow(0);
    }
  }
  else {
    TRACE2("no items in list");
  }
}


void ApplyCommandDialog::selectListElementIfOne()
{
  if (m_prevCommandsListWidget->count() == 1) {
    // Does this select it?
    m_prevCommandsListWidget->setCurrentRow(0);
  }
}


void ApplyCommandDialog::setPwdLabel(EditorWidget *editorWidget)
{
  HostName const hostName =
    editorWidget->getDocumentDirectoryHarn().hostName();

  QString const dir = toQString(editorWidget->getDocumentDirectory());

  QString labelText;
  if (hostName.isLocal()) {
    labelText =
      tr("Command to run in %1.")
        .arg(dir);
  }
  else {
    QString host = toQString(hostName.toString());
    labelText =
      tr("Command to run on %1 in %2.")
        .arg(dir)
        .arg(host);
  }
  m_pwdLabel->setText(labelText);
}


// --------------------------- private slots ---------------------------
void ApplyCommandDialog::filterChanged(QString const &) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  populateListWidget(false /*initial*/);

  // If this causes the list to have exactly one element, select it so
  // the user can then press Enter to run it without having to move to
  // the list box.
  selectListElementIfOne();

  GENERIC_CATCH_END
}


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
  if (!editorGlobal()->
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


// -------------------------- public methods ---------------------------
ApplyCommandDialog::ApplyCommandDialog(
  EditorGlobal *editorGlobal,
  EditorCommandLineFunction whichFunction)

  : ModalDialog(),
    IMEMBFP(editorGlobal),
    IMEMBFP(whichFunction)
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

  QVBoxLayout *vbox = new QVBoxLayout(this);

  // Location label.
  {
    // Initially the label is empty.  It is populated by `setPwdLabel`.
    m_pwdLabel = new QLabel();

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

  // Filter, "Copy" button, and "Delete" button.
  {
    QHBoxLayout *hbox = new QHBoxLayout();

    m_filterLabel = new QLabel(tr("&Filter"));
    SET_QOBJECT_NAME(m_filterLabel);
    hbox->addWidget(m_filterLabel);

    m_filterLineEdit = new QLineEdit();
    SET_QOBJECT_NAME(m_filterLineEdit);
    m_filterLabel->setBuddy(m_filterLineEdit);
    QObject::connect(m_filterLineEdit, &QLineEdit::textEdited,
                     this, &ApplyCommandDialog::filterChanged);
    hbox->addWidget(m_filterLineEdit);

    // Intercept certain keystrokes.
    m_filterLineEdit->installEventFilter(this);

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
  SET_QOBJECT_NAME(m_enableSubstitutionCheckBox);
  vbox->addWidget(m_enableSubstitutionCheckBox);

  if (m_whichFunction == ECLF_RUN) {
    m_prefixStderrLinesCheckBox = new QCheckBox(tr(
      "Prefix stderr lines &with \"STDERR: \""));
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
    "  - $f: Current document file name, without directory\n"
    "  - $w: Word at+after cursor\n"
    "  - $t1 ... $t9: Whitespace-separated tokens on cursor line\n";

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

  // Ensure it is at least 550x800 initially.
  resize(size().expandedTo(QSize(550, 800)));
  TRACE2("size after resize: " << toString(size()));
}


ApplyCommandDialog::~ApplyCommandDialog()
{
  TRACE1("running destructor");

  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_filterLineEdit, nullptr, this, nullptr);
  QObject::disconnect(m_copyButton, nullptr, this, nullptr);
  QObject::disconnect(m_deleteButton, nullptr, this, nullptr);
  QObject::disconnect(m_clearNewCommandButton, nullptr, this, nullptr);
}


bool ApplyCommandDialog::execForWidget(EditorWidget *editorWidget)
{
  // History to use to populate the dialog.
  CommandLineHistory const &history = getHistory();

  // Update for the directory of the document in `editorWidget`.
  setPwdLabel(editorWidget);

  // Set checkbox state.
  m_enableSubstitutionCheckBox->setChecked(history.m_useSubstitution);
  if (m_whichFunction == ECLF_RUN) {
    m_prefixStderrLinesCheckBox->setChecked(history.m_prefixStderrLines);
  }

  // Clear the line edits.
  m_newCommandLineEdit->setText("");
  m_filterLineEdit->setText("");

  // Populate the list.
  populateListWidget(true /*initial*/);

  // Ensure the list widget starts with focus.
  m_prevCommandsListWidget->setFocus();

  return execCentered(editorWidget->editorWindow());
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


bool ApplyCommandDialog::eventFilter(QObject *watched, QEvent *event)
{
  if (event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->modifiers() == Qt::NoModifier) {
      switch (keyEvent->key()) {
        case Qt::Key_Up:
          if (watched == m_filterLineEdit) {
            // Navigate up to the table.
            moveFocusToCommandsList();
            return true;           // Prevent further processing.
          }
          break;
      }
    }
  }
  return false;
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
