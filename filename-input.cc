// filename-input.cc
// code for filename-input.h

#include "filename-input.h"            // this module

// smqtutil
#include "qtutil.h"                    // toString(QString)

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // dirname, compareStringPtrs
#include "trace.h"                     // TRACE

// Qt
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QVBoxLayout>


FilenameInputDialog::FilenameInputDialog(QWidget *parent, Qt::WindowFlags f)
  : ModalDialog(parent, f),
    m_filenameLabel(NULL),
    m_filenameEdit(NULL),
    m_completionsEdit(NULL),
    m_cachedDirectory(""),
    m_cachedDirectoryEntries(),
    m_docList(NULL)
{
  this->setWindowTitle("Filename Input");

  QVBoxLayout *vbox = new QVBoxLayout();
  this->setLayout(vbox);

  m_filenameLabel = new QLabel();      // will be set later
  vbox->addWidget(m_filenameLabel);

  m_filenameEdit = new QLineEdit();    // populated later
  vbox->addWidget(m_filenameEdit);

  QObject::connect(m_filenameEdit, &QLineEdit::textEdited,
                   this, &FilenameInputDialog::on_textEdited);

  m_completionsEdit = new QTextEdit();
  vbox->addWidget(m_completionsEdit);
  m_completionsEdit->setReadOnly(true);

  this->createOkAndCancel(vbox);

  this->resize(400, 400);
}


FilenameInputDialog::~FilenameInputDialog()
{}


QString FilenameInputDialog::runDialog(
  FileTextDocumentList const *docList,
  QString initialChoice)
{
  // This is not re-entrant (for a particular dialog object).
  xassert(!m_docList);

  // Set 'm_docList' for the lifetime of this routine.
  Restorer<RCSerf<FileTextDocumentList const> >
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
  if (m_docList->findFileByNameC(filename)) {
    m_filenameLabel->setText("File already open, will switch to:");
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
      m_filenameLabel->setText("File exists:");
    }
    else {
      m_filenameLabel->setText("Is a directory:");
    }
  }
  else {
    m_filenameLabel->setText("File does not exist, will be created:");
  }
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
      sfu.getDirectoryEntries(m_cachedDirectoryEntries, dir);

      // Ensure canonical order.
      m_cachedDirectoryEntries.sort(&compareStringPtrs);
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


void FilenameInputDialog::setCompletions()
{
  string filename = toString(m_filenameEdit->text());

  SMFileUtil sfu;
  string dir, base;
  sfu.splitPath(dir, base, filename);

  // Query the dir, if needed.
  this->getEntries(dir);

  // Builder into which we will accumulate completions text.
  stringBuilder sb;

  // Include all entries for which 'base' is a prefix.
  for (int i=0; i < m_cachedDirectoryEntries.length(); i++) {
    if (prefixEquals(m_cachedDirectoryEntries[i], base)) {
      sb << m_cachedDirectoryEntries[i] << '\n';
    }
  }

  m_completionsEdit->setPlainText(toQString(sb));
}


void FilenameInputDialog::updateFeedback()
{
  this->setFilenameLabel();
  this->setCompletions();
}


void FilenameInputDialog::on_textEdited(QString const &)
{
  this->updateFeedback();
}


// EOF
