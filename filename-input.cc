// filename-input.cc
// code for filename-input.h

#include "filename-input.h"            // this module

// smqtutil
#include "qtguiutil.h"                 // messageBox
#include "qtutil.h"                    // toString(QString)

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // dirname, compareStringPtrs
#include "trace.h"                     // TRACE

// libc++
#include <algorithm>                   // std::min

// Qt
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>


FilenameInputDialog::History::History()
  : m_dialogSize(500, 800)
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
  m_filenameLabel(NULL),
  m_filenameEdit(NULL),
  m_completionsEdit(NULL),
  m_helpButton(NULL),
  m_vfsConnections(vfsConnections),
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

  m_filenameLabel = new QLabel();      // will be set later
  vbox->addWidget(m_filenameLabel);
  SET_QOBJECT_NAME(m_filenameLabel);

  m_filenameEdit = new QLineEdit();    // populated later
  vbox->addWidget(m_filenameEdit);
  SET_QOBJECT_NAME(m_filenameEdit);

  // Intercept Tab key.
  m_filenameEdit->installEventFilter(this);

  QObject::connect(m_filenameEdit, &QLineEdit::textEdited,
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
                   this, &FilenameInputDialog::on_replyAvailable);
}


FilenameInputDialog::~FilenameInputDialog()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_filenameEdit, NULL, this, NULL);
  QObject::disconnect(m_helpButton, NULL, this, NULL);
  QObject::disconnect(m_vfsConnections, nullptr, this, nullptr);
}


QString FilenameInputDialog::runDialog(
  NamedTextDocumentList const *docList,
  QString initialChoice)
{
  // This is not re-entrant (for a particular dialog object).
  xassert(!m_docList);

  // Set 'm_docList' for the lifetime of this routine.
  Restorer<RCSerf<NamedTextDocumentList const> >
    restorer(m_docList, docList);

  m_filenameEdit->setText(initialChoice);
  this->m_cachedDirectory = "";
  this->queryDirectoryIfNeeded();

  // Since the directory query is still running, this first call will
  // just update the display to show that fact.
  this->updateFeedback();

  if (this->exec()) {
    return m_filenameEdit->text();
  }
  else {
    return "";
  }
}


void FilenameInputDialog::queryDirectoryIfNeeded()
{
  string filename = toString(m_filenameEdit->text());

  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  if (m_cachedDirectory == dir) {
    // We already have the needed info.
    return;
  }

  if (m_currentRequestID != 0) {
    if (m_currentRequestDir == dir) {
      // There is already a pending request for 'dir'.
      return;
    }
    else {
      m_vfsConnections->cancelRequest(m_currentRequestID);
      m_currentRequestID = 0;
    }
  }

  std::unique_ptr<VFS_GetDirEntriesRequest> req(
    new VFS_GetDirEntriesRequest);
  req->m_path = dir;

  m_currentRequestDir = dir;
  m_vfsConnections->issueRequest(m_currentRequestID /*OUT*/,
                                 std::move(req));
}


void FilenameInputDialog::on_replyAvailable(
  VFS_Connections::RequestID requestID) NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  if (requestID == m_currentRequestID) {
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

  xassert(m_docList);
  if (m_docList->findDocumentByNameC(filename)) {
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
          if (sfu.absoluteFileExists(filename)) {
            if (m_saveAs) {
              m_filenameLabel->setText("File exists, will overwrite:");
            }
            else {
              m_filenameLabel->setText("File exists:");
            }
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

        if (prefixEquals(entryName, base)) {
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

  return prefix.substring(0, prefixLength);
}


void FilenameInputDialog::filenameCompletion()
{
  // Get the individual entries.
  ArrayStack<string> completions;
  if (!this->getCompletions(completions)) {
    return;
  }

  // Longest common prefix.
  string commonPrefix = longestCommonPrefix(completions);

  // Compare to what we have already.
  string filename = toString(m_filenameEdit->text());
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


void FilenameInputDialog::accept() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  string filename = toString(m_filenameEdit->text());

  // Normalize by passing through SMFileName, throwing away any
  // trailing slashes, and converting back to string.
  filename = SMFileName(filename).withTrailingSlash(false).toString();

  // Prompt before overwriting a file.
  SMFileUtil sfu;
  if (m_saveAs && sfu.absoluteFileExists(filename)) {
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
  if (!m_saveAs && !sfu.absoluteFileExists(filename)) {
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

  m_history->m_dialogSize = this->size();

  // Put the string back into the control so the normalization will
  // affect the result of 'runDialog'.
  m_filenameEdit->setText(toQString(filename));

  this->QDialog::accept();

  GENERIC_CATCH_END
}


// EOF
