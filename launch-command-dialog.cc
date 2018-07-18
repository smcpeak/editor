// launch-command-dialog.cc
// code for launch-command-dialog.h

#include "launch-command-dialog.h"     // this module

// smbase
#include "xassert.h"                   // xassert

// Qt
#include <QCheckBox>
#include <QVBoxLayout>


LaunchCommandDialog::LaunchCommandDialog(QWidget *parent, Qt::WindowFlags f)
  : TextInputDialog("Launch Command", parent, f),
    m_prefixStderrLines(NULL)
{
  m_prefixStderrLines =
    new QCheckBox("&Prefix stderr lines with \"STDERR: \".");
  m_vbox->insertWidget(m_vboxNextIndex++, m_prefixStderrLines);
  m_prefixStderrLines->setChecked(true);
}


LaunchCommandDialog::~LaunchCommandDialog()
{}


bool LaunchCommandDialog::prefixStderrLines() const
{
  return m_prefixStderrLines->isChecked();
}


// EOF
