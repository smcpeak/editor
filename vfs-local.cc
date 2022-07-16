// vfs-local.cc
// Code for vfs-local.h.

#include "vfs-local.h"                 // this module

// smbase
#include "exc.h"                       // xBase
#include "nonport.h"                   // getFileModificationTime
#include "sm-file-util.h"              // SMFileUtil


VFS_FileStatusReply VFS_LocalImpl::queryPath(VFS_FileStatusRequest const &req)
{
  SMFileUtil sfu;

  VFS_FileStatusReply reply;

  try {
    // Get an absolute path, using whatever is the current directory for
    // the server.
    string pathname = sfu.getAbsolutePath(req.m_path);

    // Split into directory and name components.
    sfu.splitPath(reply.m_dirName, reply.m_fileName, pathname);

    reply.m_dirExists = sfu.absolutePathExists(reply.m_dirName);
    if (reply.m_dirExists) {
      reply.m_fileKind = sfu.getFileKind(pathname);
      (void)getFileModificationTime(pathname.c_str(),
                                    reply.m_fileModificationTime);
    }
  }
  catch (xBase &x) {
    // I think none of the above calls can actually throw, but catching
    // here makes this request consistent with the others.
    reply.setFailureReason(x.why());
  }

  return reply;
}


VFS_ReadFileReply VFS_LocalImpl::readFile(
  VFS_ReadFileRequest const &req)
{
  SMFileUtil sfu;

  VFS_ReadFileReply reply;

  try {
    reply.m_contents = sfu.readFile(req.m_path);

    (void)getFileModificationTime(req.m_path.c_str(),
                                  reply.m_fileModificationTime);

    reply.m_readOnly = sfu.isReadOnly(req.m_path);

  }
  catch (xBase &x) {
    reply.setFailureReason(x.why());
  }

  return reply;
}


VFS_WriteFileReply VFS_LocalImpl::writeFile(
  VFS_WriteFileRequest const &req)
{
  SMFileUtil sfu;

  VFS_WriteFileReply reply;

  try {
    sfu.writeFile(req.m_path, req.m_contents);

    (void)getFileModificationTime(req.m_path.c_str(),
                                  reply.m_fileModificationTime);
  }
  catch (xBase &x) {
    reply.setFailureReason(x.why());
  }

  return reply;
}


VFS_DeleteFileReply VFS_LocalImpl::deleteFile(
  VFS_DeleteFileRequest const &req)
{
  SMFileUtil sfu;

  VFS_DeleteFileReply reply;

  try {
    sfu.removeFile(req.m_path);
  }
  catch (xBase &x) {
    reply.setFailureReason(x.why());
  }

  return reply;
}


// EOF
