// filename-input.cc
// code for filename-input.h

#include "filename-input.h"            // this module

// smqtutil
#include "smqtutil/qtguiutil.h"        // messageBox
#include "smqtutil/qtutil.h"           // toString(QString)
#include "smqtutil/sm-line-edit.h"     // SMLineEdit

// smbase
#include "smbase/save-restore.h"       // SetRestore
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // beginsWith
#include "smbase/strutil.h"            // dirname, compareStringPtrs
#include "smbase/trace.h"              // TRACE
#include "smbase/vector-util.h"        // vecFindIndex

// libc++
#include <algorithm>                   // std::min
#include <optional>                    // std::optional

// Qt
#include <QComboBox>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>


FilenameInputDialog::History::History()
  : m_dialogSize(800, 800)
{}


FilenameInputDialog::History::~History()
{}


FilenameInputDialog::FilenameInputDialog(
  History *history,
  VFS_Connections *vfsConnections,
  QWidget *parent,
  Qt::WindowFlags f)
:
  ModalDialog(parent, f),
  m_history(history),
  m_connectionDropDown(nullptr),
  m_filenameLabel(NULL),
  m_filenameEdit(NULL),
  m_completionsEdit(NULL),
  m_helpButton(NULL),
  m_vfsConnections(vfsConnections),
  m_hostNameList(),
  m_currentHostName(HostName::asLocal()),
  m_currentRequestID(0),
  m_currentRequestDir(""),
  m_cachedDirectory(""),
  m_cachedDirectoryEntries(),
  m_docList(NULL),
  m_saveAs(false)
{
  this->setObjectName("filename_input");
  this->setWindowTitle("Filename Input");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  {
    QHBoxLayout *connectionHBox = new QHBoxLayout();
    vbox->addLayout(connectionHBox);

    QLabel *connectionsLabel = new QLabel("&Connection:");
    connectionHBox->addWidget(connectionsLabel);

    m_connectionDropDown = new QComboBox(this);
    m_connectionDropDown->setSizePolicy(QSizePolicy::Expanding, // horiz
                                        QSizePolicy::Fixed);    // vert
    m_hostNameList = m_vfsConnections->getHostNames();
    for (HostName const &hostName : m_hostNameList) {
      m_connectionDropDown->addItem(toQString(hostName.toString()));
    }
    m_connectionDropDown->setCurrentIndex(0);
    connectionHBox->addWidget(m_connectionDropDown);
    connectionsLabel->setBuddy(m_connectionDropDown);

    // Here, 'QOverload' is used to select the overload that accepts an
    // 'int'.  (The QComboBox docs explain this, but I cannot find
    // documentation for QOverload itself.)
    QObject::connect(m_connectionDropDown,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     this,
                     &FilenameInputDialog::on_connectionIndexChanged);
  }

  m_filenameLabel = new QLabel();      // will be set later
  vbox->addWidget(m_filenameLabel);
  SET_QOBJECT_NAME(m_filenameLabel);

  m_filenameEdit = new SMLineEdit();   // Text is populated later.
  m_filenameEdit->m_selectAllUponFocus = false;
  vbox->addWidget(m_filenameEdit);
  SET_QOBJECT_NAME(m_filenameEdit);

  // Intercept Tab key.
  m_filenameEdit->installEventFilter(this);

  QObject::connect(m_filenameEdit, &SMLineEdit::textEdited,
                   this, &FilenameInputDialog::on_textEdited);

  m_completionsEdit = new QTextEdit();
  vbox->addWidget(m_completionsEdit);
  SET_QOBJECT_NAME(m_completionsEdit);
  m_completionsEdit->setReadOnly(true);

  {
    QHBoxLayout *hbox = new QHBoxLayout();
    vbox->addLayout(hbox);

    m_helpButton = new QPushButton("&Help");
    hbox->addWidget(m_helpButton);
    SET_QOBJECT_NAME(m_helpButton);
    QObject::connect(m_helpButton, &QPushButton::clicked,
                     this, &FilenameInputDialog::on_help);

    hbox->addStretch(1);

    this->createOkAndCancelButtons(hbox);
  }

  this->resize(m_history->m_dialogSize);

  QObject::connect(m_vfsConnections, &VFS_Connections::signal_replyAvailable,
                   this, &FilenameInputDialog::on_vfsReplyAvailable);
}


FilenameInputDialog::~FilenameInputDialog()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_connectionDropDown, nullptr, this, nullptr);
  QObject::disconnect(m_filenameEdit, NULL, this, NULL);
  QObject::disconnect(m_helpButton, NULL, this, NULL);
  QObject::disconnect(m_vfsConnections, nullptr, this, nullptr);
}


bool FilenameInputDialog::runDialog(
  NamedTextDocumentList const *docList,
  HostAndResourceName /*INOUT*/ &harn)
{
  // This is not re-entrant (for a particular dialog object).
  xassert(!m_docList);

  // Set 'm_docList' for the lifetime of this routine.
  SetRestore<RCSerf<NamedTextDocumentList const> >
    restorer(m_docList, docList);

  // Select the dropdown entry corresponding to the starting host name.
  if (std::optional<std::size_t> hostIndex =
        vecFindIndex(m_hostNameList, harn.hostName())) {
    m_connectionDropDown->setCurrentIndex(convertNumber<int>(*hostIndex));
    m_currentHostName = harn.hostName();

    m_filenameEdit->setText(toQString(harn.resourceName()));
  }
  else {
    // This happens if 'harn' comes from a host that has been
    // disconnected.  Switch to a valid host.
    on_connectionIndexChanged(0);
  }

  // We should have now established the invariant that the current host
  // name is valid.
  xassert(m_vfsConnections->isValid(m_currentHostName));

  // Set the focus on the text edit so I can start typing immediately.
  m_filenameEdit->setFocus(Qt::OtherFocusReason);

  clearCacheAndReQuery();

  // Show the dialog, returning once it is closed.
  bool ret = this->exec();

  // One way a request could still be pending is if the user types a
  // file name and hits Enter before we can populate the list of file
  // names.
  this->cancelCurrentRequestIfAny();

  if (ret) {
    harn = HostAndResourceName(
      m_currentHostName,
      toString(m_filenameEdit->text())
    );
  }

  return ret;
}


void FilenameInputDialog::clearCacheAndReQuery()
{
  m_cachedDirectory = "";
  queryDirectoryIfNeeded();

  // Since the directory query is still running, this first call will
  // just update the display to show that fact.
  updateFeedback();
}


void FilenameInputDialog::queryDirectoryIfNeeded()
{
  string filename = toString(m_filenameEdit->text());

  if (filename.empty()) {
    // Ignore empty file name.
    return;
  }

  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  if (m_cachedDirectory == dir) {
    // We already have the needed info.
    TRACE("FilenameInputDialog",
      "queryDirectoryIfNeeded: already cached dir is: " << dir);

    // If there is a request, cancel it, since its arrival will cause
    // the cached info for 'dir' to be discarded.
    cancelCurrentRequestIfAny();
    return;
  }

  if (m_currentRequestID != 0) {
    if (m_currentRequestDir == dir) {
      // There is already a pending request for 'dir'.
      TRACE("FilenameInputDialog",
        "queryDirectoryIfNeeded: request is pending for: " << dir);
      return;
    }
    else {
      cancelCurrentRequestIfAny();
    }
  }

  TRACE("FilenameInputDialog",
    "queryDirectoryIfNeeded: will issue request for: " << dir);

  std::unique_ptr<VFS_GetDirEntriesRequest> req(
    new VFS_GetDirEntriesRequest);
  req->m_path = dir;

  m_currentRequestDir = dir;
  m_vfsConnections->issueRequest(m_currentRequestID /*OUT*/,
                                 m_currentHostName,
                                 std::move(req));
}


void FilenameInputDialog::cancelCurrentRequestIfAny()
{
  if (m_currentRequestID != 0) {
    TRACE("FilenameInputDialog",
      "canceling outstanding request for: " << m_currentRequestDir);

    m_vfsConnections->cancelRequest(m_currentRequestID);
    m_currentRequestID = 0;
    m_currentRequestDir = "";
  }
}


void FilenameInputDialog::on_vfsReplyAvailable(
  VFS_Connections::RequestID requestID) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (requestID == m_currentRequestID) {
    TRACE("FilenameInputDialog",
      "on_vfsReplyAvailable: reply arrived for: " << m_currentRequestDir);

    std::unique_ptr<VFS_Message> reply(
      m_vfsConnections->takeReply(requestID));
    VFS_GetDirEntriesReply const *gde = reply->asGetDirEntriesReplyC();

    m_cachedDirectory = m_currentRequestDir;
    if (gde->m_success) {
      m_cachedDirectoryEntries = gde->m_entries;
    }
    else {
      // Assume the directory simply does not exist, and so show an
      // empty list.
      //
      // TODO: Distinguish between non-existence and other errors.
      m_cachedDirectoryEntries.clear();
    }

    m_currentRequestID = 0;
    m_currentRequestDir = "";

    updateFeedback();
  }

  GENERIC_CATCH_END
}


void FilenameInputDialog::setFilenameLabel()
{
  string filename = toString(m_filenameEdit->text());

  if (filename.empty()) {
    m_filenameLabel->setText("File path is empty.");
    return;
  }

  xassert(m_docList);
  if (m_docList->findDocumentByNameC(
        DocumentName::fromFilename(m_currentHostName, filename))) {
    if (m_saveAs) {
      m_filenameLabel->setText("File already open, CANNOT save as this name:");
    }
    else {
      m_filenameLabel->setText("File already open, will switch to:");
    }
    return;
  }

  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  if (dir == m_cachedDirectory) {
    if (m_cachedDirectoryEntries.empty()) {
      m_filenameLabel->setText(qstringb(
        "Directory does not exist: " << dir));
    }
    else if (base == "") {
      m_filenameLabel->setText("File name is empty.");
    }
    else {
      SMFileUtil::FileKind fileKind =
        lookupInCachedDirectoryEntries(base);
      switch (fileKind) {
        case SMFileUtil::FK_REGULAR:
          if (m_saveAs) {
            m_filenameLabel->setText("File exists, will overwrite:");
          }
          else {
            m_filenameLabel->setText("File exists:");
          }
          break;

        case SMFileUtil::FK_DIRECTORY:
          m_filenameLabel->setText("Is a directory:");
          break;

        case SMFileUtil::FK_NONE:
          m_filenameLabel->setText("File does not exist, will be created:");
          break;

        default:
          m_filenameLabel->setText(qstringb(
            "File kind is " << toString(fileKind) << ":"));
          break;
      }
    }
  }

  else if (dir == m_currentRequestDir) {
    m_filenameLabel->setText(qstringb("Loading directory: " << dir));
  }

  else {
    m_filenameLabel->setText("Directory information desynchronized?");
  }
}


SMFileUtil::FileKind
FilenameInputDialog::lookupInCachedDirectoryEntries(string const &name)
{
  for (SMFileUtil::DirEntryInfo const &entry : m_cachedDirectoryEntries) {
    if (entry.m_name == name) {
      return entry.m_kind;
    }
  }
  return SMFileUtil::FK_NONE;
}


// Return true if 'dei' is neither the "." nor ".." directories.
//
// TODO: Invert the sense of this test.
static bool isNotDotOrDotDot(SMFileUtil::DirEntryInfo const &dei)
{
  return !(dei.m_kind == SMFileUtil::FK_DIRECTORY &&
           (dei.m_name == "." || dei.m_name == ".."));
}


bool FilenameInputDialog::getCompletions(
  ArrayStack<string> /*OUT*/ &completions)
{
  string filename = toString(m_filenameEdit->text());

  if (filename.empty()) {
    // No completions for an empty path.
    return true;
  }

  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  // Get all entries for which 'base' is a prefix.
  completions.clear();
  if (dir == m_cachedDirectory) {
    for (SMFileUtil::DirEntryInfo const &entry : m_cachedDirectoryEntries) {
      if (isNotDotOrDotDot(entry)) {
        // Add a slash to every directory entry.
        string entryName = (entry.m_kind == SMFileUtil::FK_DIRECTORY)?
          stringb(entry.m_name << "/") :
          entry.m_name;

        if (beginsWith(entryName, base)) {
          completions.push(entryName);
        }
      }
    }

    return true;
  }
  else {
    // Wrong directory.
    return false;
  }
}


void FilenameInputDialog::setCompletions()
{
  // Get the individual entries.
  ArrayStack<string> completions;
  if (!this->getCompletions(completions)) {
    m_completionsEdit->setPlainText("Loading ...");
    return;
  }

  // Assemble them into one string.
  stringBuilder sb;
  for (int i=0; i < completions.length(); i++) {
    string const &entry = completions[i];
    sb << entry << '\n';
  }

  // Put that into the control.
  m_completionsEdit->setPlainText(toQString(sb));
}


void FilenameInputDialog::updateFeedback()
{
  this->setFilenameLabel();
  this->setCompletions();
}


static int commonPrefixLength(string const &a, string const &b)
{
  int aLen = a.length();
  int bLen = b.length();
  int i = 0;
  while (i < aLen && i < bLen && a[i] == b[i]) {
    i++;
  }
  return i;
}


// Return the longest string that is a prefix of every string in
// 'strings'.  If 'strings' is empty, return the empty string.
static string longestCommonPrefix(ArrayStack<string> const &strings)
{
  if (strings.isEmpty()) {
    return "";
  }

  // Starting candidate prefix.  The actual longest common prefix must
  // be a prefix of this one.
  string prefix = strings[0];

  // Length of longest common prefix.  Always less than or equal to
  // the length of 'prefix'.
  int prefixLength = prefix.length();

  // Compare to each successive string, reducing 'prefixLength' as we go.
  for (int i=1; i < strings.length(); i++) {
    int plen = commonPrefixLength(prefix, strings[i]);
    prefixLength = std::min(prefixLength, plen);
    if (prefixLength == 0) {
      break;       // Early exit, cannot get any smaller.
    }
  }

  return prefix.substr(0, prefixLength);
}


void FilenameInputDialog::filenameCompletion()
{
  string filename = toString(m_filenameEdit->text());

  if (filename.empty()) {
    // Don't try to complete an empty file name.
    return;
  }

  // Get the individual entries.
  ArrayStack<string> completions;
  if (!this->getCompletions(completions)) {
    return;
  }

  // Longest common prefix.
  string commonPrefix = longestCommonPrefix(completions);

  // Compare to what we have already.
  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  if (commonPrefix.length() > base.length()) {
    TRACE("FilenameInputDialog", "completed prefix: " << commonPrefix);
    m_filenameEdit->setText(qstringb(dir << commonPrefix));
    this->queryDirectoryIfNeeded();
    this->updateFeedback();
  }
}


// Based on: http://doc.qt.io/qt-5/eventsandfilters.html.
bool FilenameInputDialog::eventFilter(QObject *watched, QEvent *event)
{
  if (watched == m_filenameEdit && event->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
    if (keyEvent->modifiers() == Qt::NoModifier) {
      switch (keyEvent->key()) {
        case Qt::Key_Tab:
          // Special tab handling.
          TRACE("FilenameInputDialog", "saw Tab press");
          this->filenameCompletion();
          return true;           // Prevent further processing.

        case Qt::Key_PageUp:
        case Qt::Key_PageDown: {
          TRACE("FilenameInputDialog", "page up/down");
          int sense = (keyEvent->key() == Qt::Key_PageDown)? +1 : -1;
          QScrollBar *scroll = m_completionsEdit->verticalScrollBar();
          scroll->setValue(scroll->value() + scroll->pageStep() * sense);
          return true;
        }
      }
    }
  }
  return false;
}


bool FilenameInputDialog::wantResizeEventsRecorded()
{
  return true;
}


void FilenameInputDialog::on_connectionIndexChanged(int index) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  TRACE("FilenameInputDialog", "on_connectionIndexChanged(" << index << ")");

  xassert(0 <= index && index < (int)m_hostNameList.size());
  HostName hostName = m_hostNameList[index];
  TRACE("FilenameInputDialog", "  hostName: " << hostName);

  // Change current host name.
  int index = m_connectionDropDown->currentIndex();
  xassert(0 <= index && index < (int)m_hostNameList.size());
  m_currentHostName = m_hostNameList.at(index);

  // Change directory to host's starting directory.
  if (m_vfsConnections->isOrWasConnected(m_currentHostName)) {
    string dir = m_vfsConnections->getStartingDirectory(m_currentHostName);
    m_filenameEdit->setText(toQString(dir));
  }
  else {
    // It's possible we have an entry for a host that never connected.
    // Just leave the file name box as is.
  }

  // To account for the changed host and directory, invalidate the cache
  // and re-query.
  clearCacheAndReQuery();

  GENERIC_CATCH_END
}


void FilenameInputDialog::on_textEdited(QString const &) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  // Editing the text could change the directory.
  this->queryDirectoryIfNeeded();

  this->updateFeedback();

  GENERIC_CATCH_END
}


void FilenameInputDialog::on_help() NOEXCEPT
{
  GENERIC_CATCH_BEGIN
  messageBox(this, "Help",
    qstringb(
      (m_saveAs?
        "Type a file name to choose the name to save as.\n" :
        "Type a file name to create or open or switch to it.\n") <<
      "\n"
      "Tab: Complete partial file or directory name.\n"
      "PageUp/Down: Scroll the completions window.\n"));
  GENERIC_CATCH_END
}


bool FilenameInputDialog::knowFileExistence(
  bool /*OUT*/ &exists, string const &filename)
{
  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  if (dir == m_cachedDirectory) {
    SMFileUtil::FileKind fileKind =
      lookupInCachedDirectoryEntries(base);
    exists = (fileKind == SMFileUtil::FK_REGULAR);
    return true;
  }
  else {
    // The directory is not cached.
    return false;
  }
}


void FilenameInputDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  string filename = toString(m_filenameEdit->text());

  if (filename.empty()) {
    // Silently ignore Ok press while name is empty.
    return;
  }

  // Normalize by passing through SMFileName, throwing away any
  // trailing slashes, and converting back to string.
  filename = SMFileName(filename).withTrailingSlash(false).toString();

  // See if the file exists in order to provide extra prompting.
  bool exists;
  if (knowFileExistence(exists /*OUT*/, filename)) {
    // Prompt before overwriting a file.
    if (m_saveAs && exists) {
      QMessageBox box(this);
      box.setWindowTitle("Overwrite Existing File?");
      box.setText(qstringb(
        "Overwrite existing file \"" << filename << "\"?"));
      box.addButton(QMessageBox::Yes);
      box.addButton(QMessageBox::Cancel);
      if (box.exec() != QMessageBox::Yes) {
        // Bail out without closing.
        return;
      }
    }

    // If we are loading a file, and the file does not exist, prompt
    // first.  Without this, I often hit Enter a bit too fast, intending
    // to open a file but instead creating a new one, which is annoying
    // because then I have to re-open the dialog and navigate to the
    // intended location a second time.
    if (!m_saveAs && !exists) {
      QMessageBox box(this);
      box.setObjectName("createFilePrompt");
      box.setWindowTitle("Create New File?");
      box.setText(qstringb(
        "Create new file \"" << filename << "\"?"));
      box.addButton(QMessageBox::Yes);
      box.addButton(QMessageBox::Cancel);
      if (box.exec() != QMessageBox::Yes) {
        // Bail out without closing.
        return;
      }
    }
  }
  else {
    // If we don't know, then just bypass the extra prompting.
  }

  m_history->m_dialogSize = this->size();

  // Put the string back into the control so the normalization will
  // affect the result of 'runDialog'.
  m_filenameEdit->setText(toQString(filename));

  this->QDialog::accept();

  GENERIC_CATCH_END
}


// EOF
