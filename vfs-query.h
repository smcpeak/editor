// vfs-query.h
// FileSystemQuery class.

#ifndef EDITOR_VFS_QUERY_H
#define EDITOR_VFS_QUERY_H

// smbase
#include "sm-file-util.h"              // SMFileUtil
#include "str.h"                       // string

// qt
#include <QObject>
#include <QTimer>

// libc
#include <stdint.h>                    // int64_t


// Class to issue asynchronous file system queries that might go to a
// remote network resource.
class FileSystemQuery : public QObject {
  Q_OBJECT

private:     // data
  // Timer for simulating network delays.
  QTimer m_timer;

  // File path we are instructed to query.
  string m_pathname;

public:      // data
  // When non-zero, introduce an artificial delay of this many
  // milliseconds in order to simulate network delay.  Default is 0.
  int m_simulatedDelayMS;

  // True if the 'dir' exists and is a directory.
  bool m_dirExists;

  // Existence and kind of 'base'.
  SMFileUtil::FileKind m_baseKind;

  // If 'base' exists, its unix modification time.
  int64_t m_baseModificationTime;

private:     // methods
  // Do the query for 'm_pathname' against the local file system.
  void doLocalQuery();

public:      // methods
  // Make an object to query the local file system.
  FileSystemQuery();

  virtual ~FileSystemQuery();

  // Divide 'pathname' into 'dir' and 'base' components, and report
  // whether 'dir' exists and, if so, the existence and type of 'base'.
  //
  // When the result is ready, 'm_dirExists' (etc.) will be populated
  // and 'signal_resultsReady' emitted.
  void queryPath(string pathname);

  // Cancel an outstanding request.  This should be done before
  // destroying the object if a request is pending.
  void cancelRequest();

Q_SIGNALS:
  // Emitted when the result of the query is available.
  void signal_resultsReady();

protected Q_SLOTS:
  // Handler for QTimer signal.
  void on_timeout() NOEXCEPT;
};

#endif // EDITOR_VFS_QUERY_H
