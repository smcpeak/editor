// process-watcher.cc
// code for process-watcher.h

#include "process-watcher.h"           // this module

// smqtutil
#include "qtutil.h"                    // toString(QString)


ProcessWatcher::ProcessWatcher(NamedTextDocument *doc)
  : QObject(),
    m_namedDoc(doc),
    m_commandRunner(),
    m_startTime(getCurrentUnixTime()),
    m_prefixStderrLines(true)
{
  m_namedDoc->setDocumentProcessStatus(DPS_RUNNING);

  QObject::connect(&m_commandRunner, &CommandRunner::signal_outputLineReady,
                   this,              &ProcessWatcher::slot_outputLineReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_errorLineReady,
                   this,              &ProcessWatcher::slot_errorLineReady);
  QObject::connect(&m_commandRunner, &CommandRunner::signal_processTerminated,
                   this,              &ProcessWatcher::slot_processTerminated);
}


ProcessWatcher::~ProcessWatcher()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_commandRunner, NULL, this, NULL);
}


void ProcessWatcher::slot_outputLineReady() NOEXCEPT
{
  while (m_commandRunner.hasOutputLine()) {
    QString line = m_commandRunner.getOutputLine();
    if (m_namedDoc) {
      string s(toString(line));
      m_namedDoc->appendString(s);
    }
    else {
      // This is an interesting situation: we are getting output, but
      // do not have an associated document.  We make sure to retrieve
      // and discard the output so the output buffer does not grow
      // indefinitely (QProcess does not appear to have a bound on what
      // it will accumulate in memory; I've gotten it to 100 MB).
      //
      // One way this could happen is if the child process is unkillable
      // but keeps delivering output data.  In an ideal world, we could
      // close all handles related to such a process, forcing an end to
      // the IPC even if the process continues.  But QProcess does not
      // offer a way to do that (without incurring a 30s process-wide
      // hang), so we're stuck spending cycles servicing that output.
      // See doc/qprocess-hangs.txt.
      //
      // If the child does eventually die, we will get a call on
      // slot_processTerminated and be able to reap it normally.
    }
  }
}


void ProcessWatcher::slot_errorLineReady() NOEXCEPT
{
  while (m_commandRunner.hasErrorLine()) {
    QString line = m_commandRunner.getErrorLine();
    if (m_namedDoc) {
      string s(toString(line));
      if (m_prefixStderrLines) {
        // This is a crude indicator of stdout versus stderr.  I would
        // like to communicate this differently somehow.
        m_namedDoc->appendCStr("STDERR: ");
      }
      m_namedDoc->appendString(s);
    }
  }
}


void ProcessWatcher::slot_processTerminated() NOEXCEPT
{
  if (m_namedDoc) {
    m_namedDoc->appendCStr("\n");

    if (m_commandRunner.getFailed()) {
      m_namedDoc->appendString(stringb("Failed: " <<
        toString(m_commandRunner.getErrorMessage()) << '\n'));
    }
    else {
      m_namedDoc->appendString(stringb("Exit code: " <<
        m_commandRunner.getExitCode() << '\n'));
    }

    UnixTime endTime = getCurrentUnixTime();
    UnixTime elapsed = endTime - m_startTime;
    m_namedDoc->appendString(stringb(
      "Elapsed: " << elapsed << " s\n"));

    DateTimeSeconds date;
    date.fromUnixTime(endTime, getLocalTzOffsetMinutes());
    m_namedDoc->appendString(stringb(
      "Finished at " << date.dateTimeString() << "\n"));

    // I do this at the end so that observers see the changes above as
    // happening while the process is still running, and hence
    // understand the user did not directly make them.
    m_namedDoc->setDocumentProcessStatus(DPS_FINISHED);
  }

  Q_EMIT signal_processTerminated(this);
}


// EOF
