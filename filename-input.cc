// filename-input.cc
// code for filename-input.h

#include "filename-input.h"            // this module

// smqtutil
#include "qtutil.h"                    // toString(QString)

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // dirname

// Qt
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>


FilenameInputDialog::FilenameInputDialog(QWidget *parent, Qt::WindowFlags f)
  : ModalDialog(parent, f),
    m_filenameLabel(NULL),
    m_filenameEdit(NULL)
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

  this->createOkAndCancel(vbox);

  // Start with 400 width and minimum height.
  this->resize(400, 80);
}


FilenameInputDialog::~FilenameInputDialog()
{}


QString FilenameInputDialog::runDialog(QString initialChoice)
{
  m_filenameEdit->setText(initialChoice);
  this->setFilenameLabel();

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

  SMFileUtil sfu;

  // TODO: These file system queries have some issues:
  //
  //  * With "D:/", complains about "./" not existing.
  //
  //  * With "D:/wrk/", says "File exists", even though it is a
  //    directory.

  // Final '/' is a hack for when the "directory" is just a drive
  // letter and colon.  It makes it see it as a valid directory.
  string dir = stringb(dirname(filename) << '/');

  if (!sfu.absolutePathExists(dir)) {
    m_filenameLabel->setText(qstringb(
      "Directory does not exist: " << dir));
    return;
  }

  if (!sfu.absolutePathExists(filename)) {
    m_filenameLabel->setText("File does not exist, will be created:");
    return;
  }

  m_filenameLabel->setText("File exists:");
}


void FilenameInputDialog::on_textEdited(QString const &)
{
  this->setFilenameLabel();
}


// EOF
