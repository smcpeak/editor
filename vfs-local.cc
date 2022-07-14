// vfs-local.cc
// Code for vfs-local.h.

#include "vfs-local.h"                 // this module

// smbase
#include "nonport.h"                   // getFileModificationTime
#include "sm-file-util.h"              // SMFileUtil


VFS_PathReply VFS_LocalImpl::queryPath(VFS_PathRequest const &req)
{
  SMFileUtil sfu;

  // Get an absolute path, using whatever is the current directory for
  // the server.
  string pathname = sfu.getAbsolutePath(req.m_path);

  // Split into directory and name components.
  VFS_PathReply reply;
  sfu.splitPath(reply.m_dirName, reply.m_fileName, pathname);

  reply.m_dirExists = sfu.absolutePathExists(reply.m_dirName);
  if (reply.m_dirExists) {
    reply.m_fileKind = sfu.getFileKind(pathname);
    (void)getFileModificationTime(pathname.c_str(),
                                  reply.m_fileModificationTime);
  }

  return reply;
}


// EOF
