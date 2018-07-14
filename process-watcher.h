// process-watcher.h
// ProcessWatcher.

#ifndef PROCESS_WATCHER_H
#define PROCESS_WATCHER_H

// editor
#include "command-runner.h"            // CommandRunner
#include "named-td.h"                  // NamedTextDocument

// smbase
#include "datetime.h"                  // UnixTime


// Monitor a child process and feed the output to a TextDocumentEditor.
//
// This class basically just relays data from CommandRunner to
// NamedTextDocument.
class ProcessWatcher : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(ProcessWatcher);

public:      // data
  // The document to receive the data.  Although initially it must be
  // non-NULL, it can later be set to NULL in order to discard any
  // extra output while the underlying process is killed.
  RCSerf<NamedTextDocument> m_namedDoc;

  // The child process producing it.
  CommandRunner m_commandRunner;

  // Point in time when process started.
  UnixTime m_startTime;

public:      // funcs
  ProcessWatcher(NamedTextDocument *doc);
  ~ProcessWatcher();

Q_SIGNALS:
  // Emitted when the process terminates.  This is meant to notify the
  // client to clean up the watcher.
  void signal_processTerminated(ProcessWatcher *watcher);

private Q_SLOTS:
  // Handlers for CommandRunner signals.
  void slot_outputLineReady() NOEXCEPT;
  void slot_errorLineReady() NOEXCEPT;
  void slot_processTerminated() NOEXCEPT;
};


#endif // PROCESS_WATCHER_H
