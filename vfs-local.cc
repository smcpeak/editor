// vfs-local.cc
// Code for vfs-local.h.

#include "vfs-local.h"                 // this module

// smbase
#include "exc.h"                       // xBase
#include "nonport.h"                   // getFileModificationTime
#include "sm-file-util.h"              // SMFileUtil


// Catch block for VFS_LocalImpl methods that service a PathRequest.
// It sets the failure code and reason of 'reply'.
#define PATH_REQUEST_CATCH_BLOCK                           \
  catch (xSysError &x) {                                   \
    reply.setFailureReason(x.reason, x.why());             \
  }                                                        \
  catch (xBase &x) {                                       \
    reply.setFailureReason(xSysError::R_UNKNOWN, x.why()); \
  }


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
  PATH_REQUEST_CATCH_BLOCK

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
  PATH_REQUEST_CATCH_BLOCK

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
  PATH_REQUEST_CATCH_BLOCK

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
  PATH_REQUEST_CATCH_BLOCK

  return reply;
}


VFS_GetDirEntriesReply VFS_LocalImpl::getDirEntries(
  VFS_GetDirEntriesRequest const &req)
{
  SMFileUtil sfu;

  VFS_GetDirEntriesReply reply;

  try {
    ArrayStack<SMFileUtil::DirEntryInfo> entries;
    sfu.getSortedDirectoryEntries(entries, req.m_path);

    // Copy into std::vector.
    for (int i=0; i < entries.length(); i++) {
      reply.m_entries.push_back(entries[i]);
    }
  }
  PATH_REQUEST_CATCH_BLOCK

  return reply;
}


// EOF
