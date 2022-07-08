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


FilenameInputDialog::FilenameInputDialog(History *history,
                                         QWidget *parent, Qt::WindowFlags f)
  : ModalDialog(parent, f),
    m_history(history),
    m_filenameLabel(NULL),
    m_filenameEdit(NULL),
    m_completionsEdit(NULL),
    m_helpButton(NULL),
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
}


FilenameInputDialog::~FilenameInputDialog()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(m_filenameEdit, NULL, this, NULL);
  QObject::disconnect(m_helpButton, NULL, this, NULL);
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
  this->updateFeedback();

  if (this->exec()) {
    return m_filenameEdit->text();
  }
  else {
    return "";
  }
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

  if (!sfu.absolutePathExists(dir)) {
    m_filenameLabel->setText(qstringb(
      "Directory does not exist: " << dir));
    return;
  }

  if (sfu.absolutePathExists(filename)) {
    if (sfu.absoluteFileExists(filename)) {
      if (m_saveAs) {
        m_filenameLabel->setText("File exists, will overwrite:");
      }
      else {
        m_filenameLabel->setText("File exists:");
      }
    }
    else {
      m_filenameLabel->setText("Is a directory:");
    }
  }
  else {
    m_filenameLabel->setText("File does not exist, will be created:");
  }
}


// Return true if 'dei' is neither the "." nor ".." directories.
static bool isNotDotOrDotDot(SMFileUtil::DirEntryInfo const &dei)
{
  return !(dei.m_kind == SMFileUtil::FK_DIRECTORY &&
           (dei.m_name == "." || dei.m_name == ".."));
}

void FilenameInputDialog::getEntries(string const &dir)
{
  if (dir == m_cachedDirectory) {
    // Already cached.
    return;
  }

  // Just for safety, clear cache key while we repopulate.
  m_cachedDirectory = "";

  SMFileUtil sfu;
  if (sfu.absolutePathExists(dir)) {
    try {
      TRACE("FilenameInputDialog", "querying dir: " << dir);
      ArrayStack<SMFileUtil::DirEntryInfo> entries;
      sfu.getSortedDirectoryEntries(entries, dir);

      // Remove the "." and ".." entries.  I never want to open these,
      // and their presence prevents me from simply hitting Tab
      // repeatedly to drill down into structures that only have one
      // subdirectory at each level.
      applyFilter(entries, isNotDotOrDotDot);

      // Copy them into the cache.
      m_cachedDirectoryEntries.clear();
      for (int i=0; i < entries.length(); i++) {
        if (entries[i].m_kind == SMFileUtil::FK_DIRECTORY) {
          // Add a slash to the name.  This both indicates to the user
          // the type of entity it is and also causes Tab completion to
          // add the slash when it is unambiguous, thus automatically
          // transitioning into the directory.
          m_cachedDirectoryEntries.push(stringb(entries[i].m_name << '/'));
        }
        else {
          m_cachedDirectoryEntries.push(entries[i].m_name);
        }
      }
    }
    catch (...) {
      // In case of failure, clear completions and bail.
      m_cachedDirectoryEntries.clear();
    }
  }
  else {
    m_cachedDirectoryEntries.clear();
  }

  // Ok, cache is ready.
  m_cachedDirectory = dir;
}


void FilenameInputDialog::getCompletions(
  ArrayStack<string> /*OUT*/ &completions)
{
  string filename = toString(m_filenameEdit->text());

  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  // Query the dir, if needed.
  this->getEntries(dir);

  // Get all entries for which 'base' is a prefix.
  completions.clear();
  for (int i=0; i < m_cachedDirectoryEntries.length(); i++) {
    string const &entry = m_cachedDirectoryEntries[i];
    if (prefixEquals(entry, base)) {
      completions.push(entry);
    }
  }
}


void FilenameInputDialog::setCompletions()
{
  // Get the individual entries.
  ArrayStack<string> completions;
  this->getCompletions(completions);

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
  this->getCompletions(completions);

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
