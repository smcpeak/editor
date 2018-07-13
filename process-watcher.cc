// process-watcher.cc
// code for process-watcher.h

#include "process-watcher.h"           // this module

// smqtutil
#include "qtutil.h"                    // toString(QString)


ProcessWatcher::ProcessWatcher(FileTextDocument *doc)
  : QObject(),
    m_fileDoc(doc),
    m_commandRunner()
{
  m_fileDoc->setIsProcessOutput(true);

  QObject::connect(&m_commandRunner, &CommandRunner::signal_outputLineReady,
                   this,              &ProcessWatcher::slot_outputLineReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_errorLineReady,
                   this,              &ProcessWatcher::slot_errorLineReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_processTerminated,
                   this,              &ProcessWatcher::slot_processTerminated);
}


ProcessWatcher::~ProcessWatcher()
{}


void ProcessWatcher::slot_outputLineReady() NOEXCEPT
{
  while (m_commandRunner.hasOutputLine()) {
    QString line = m_commandRunner.getOutputLine();
    if (m_fileDoc) {
      string s(toString(line));
      m_fileDoc->appendString(s);
    }
  }
}


void ProcessWatcher::slot_errorLineReady() NOEXCEPT
{
  while (m_commandRunner.hasErrorLine()) {
    QString line = m_commandRunner.getErrorLine();
    if (m_fileDoc) {
      string s(toString(line));
      // This is a crude indicator of stdout versus stderr.  I would
      // like to communicate this differently somehow.
      m_fileDoc->appendCStr("STDERR: ");
      m_fileDoc->appendString(s);
    }
  }
}


void ProcessWatcher::slot_processTerminated() NOEXCEPT
{
  if (m_fileDoc) {
    m_fileDoc->appendCStr("\n");

    if (m_commandRunner.getFailed()) {
      m_fileDoc->appendString(stringb("Failed: " <<
        toString(m_commandRunner.getErrorMessage()) << '\n'));
    }
    else {
      m_fileDoc->appendString(stringb("Exit code: " <<
        m_commandRunner.getExitCode() << '\n'));
    }
  }

  Q_EMIT signal_processTerminated(this);
}


// EOF