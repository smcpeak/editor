// launch-command-dialog.cc
// code for launch-command-dialog.h

#include "launch-command-dialog.h"     // this module

// smqtutil
#include "qtutil.h"                    // SET_QOBJECT_NAME

// smbase
#include "xassert.h"                   // xassert

// Qt
#include <QCheckBox>
#include <QVBoxLayout>


LaunchCommandDialog::LaunchCommandDialog(QWidget *parent, Qt::WindowFlags f)
  : TextInputDialog("Launch Command", parent, f),
    m_enableSubstitutionCheckbox(NULL),
    m_prefixStderrLines(NULL)
{
  this->setObjectName("LaunchCommandDialog");

  m_enableSubstitutionCheckbox =
    new QCheckBox("Enable &substitution (see help)");
  m_vbox->insertWidget(m_vboxNextIndex++, m_enableSubstitutionCheckbox);
  SET_QOBJECT_NAME(m_enableSubstitutionCheckbox);
  m_enableSubstitutionCheckbox->setChecked(true);

  m_prefixStderrLines =
    new QCheckBox("&Prefix stderr lines with \"STDERR: \"");
  m_vbox->insertWidget(m_vboxNextIndex++, m_prefixStderrLines);
  SET_QOBJECT_NAME(m_prefixStderrLines);
  m_prefixStderrLines->setChecked(false);

  createHelpButton();
  m_helpText =
    "This spawns a process with the given command line in the "
    "directory containing the current file, and creates a new "
    "editor document containing its output (or replaces one, if "
    "one already exists with the exact same command line and "
    "directory)."
    "\n\n"
    "If \"Enable substitution\" is checked, then the following "
    "substitutions will be performed on the command line before "
    "executing:\n"
    "\n\n"
    "  - $f: Current document file name, without directory\n"
    "\n\n"
    "If \"Prefix stderr\" is checked, then the command will be run "
    "with stdout and stderr going to separate streams, and stderr "
    "lines will have \"STDERR: \" prefixed for identification.  "
    "However, this means the precise temporal interleaving between "
    "output and error is lost.";
}


LaunchCommandDialog::~LaunchCommandDialog()
{}


bool LaunchCommandDialog::enableSubstitution() const
{
  return m_enableSubstitutionCheckbox->isChecked();
}


bool LaunchCommandDialog::prefixStderrLines() const
{
  return m_prefixStderrLines->isChecked();
}


// EOF
