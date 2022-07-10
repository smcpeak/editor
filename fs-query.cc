// fs-query.cc
// Code for fs-query.h.

#include "fs-query.h"                  // this module

// smbase
#include "exc.h"                       // GENERIC_CATCH_BEGIN/END
#include "nonport.h"                   // getFileModificationTime


FileSystemQuery::FileSystemQuery()
  : m_timer(this /*parent*/),
    m_pathname(),
    m_simulatedDelayMS(0),
    m_dirExists(false),
    m_baseKind(SMFileUtil::FK_NONE),
    m_baseModificationTime(0)
{
  m_timer.setSingleShot(true);

  QObject::connect(&m_timer, &QTimer::timeout,
                   this, &FileSystemQuery::on_timeout);
}


FileSystemQuery::~FileSystemQuery()
{
  // See doc/signals-and-dtors.txt.
  QObject::disconnect(&m_timer, nullptr, this, nullptr);
}


void FileSystemQuery::doLocalQuery()
{
  SMFileUtil sfu;
  string pathname = sfu.getAbsolutePath(m_pathname);

  string dir, base;
  sfu.splitPath(dir, base, pathname);

  if (!sfu.absolutePathExists(dir)) {
    m_dirExists = false;
  }

  else {
    m_baseKind = sfu.getFileKind(pathname);
    (void)getFileModificationTime(pathname.c_str(), m_baseModificationTime);
  }

  Q_EMIT signal_resultsReady();
}


void FileSystemQuery::queryPath(string pathname)
{
  m_pathname = pathname;

  if (m_simulatedDelayMS) {
    m_timer.start(m_simulatedDelayMS);
  }
  else {
    doLocalQuery();
  }
}


void FileSystemQuery::cancelRequest()
{
  m_timer.stop();
}


void FileSystemQuery::on_timeout() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  doLocalQuery();

  GENERIC_CATCH_END
}


// EOF
