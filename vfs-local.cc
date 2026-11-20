// vfs-local.cc
// Code for vfs-local.h.

#include "vfs-local.h"                           // this module

// smbase
#include "smbase/exc.h"                          // smbase::XBase
#include "smbase/nonport.h"                      // getFileModificationTime
#include "smbase/portable-error-code.h"          // smbase::PortableErrorCode
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/syserr.h"                       // smbase::XSysError

using namespace smbase;


// Catch block for VFS_LocalImpl methods that service a PathRequest.
// It sets the failure code and reason of 'reply'.
#define PATH_REQUEST_CATCH_BLOCK                                     \
  catch (XSysError &x) {                                             \
    reply.setFailureReason(x.getPortableErrorCode(), x.why());       \
  }                                                                  \
  catch (XBase &x) {                                                 \
    reply.setFailureReason(PortableErrorCode::PEC_UNKNOWN, x.why()); \
  }


RFA_Msg_FileStatusReply VFS_LocalImpl::queryPath(RFA_Msg_FileStatusRequest const &req)
{
  SMFileUtil sfu;

  RFA_Msg_FileStatusReply reply;

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


RFA_Msg_ReadFileReply VFS_LocalImpl::readFile(
  RFA_Msg_ReadFileRequest const &req)
{
  SMFileUtil sfu;

  RFA_Msg_ReadFileReply reply;

  try {
    reply.m_contents = sfu.readFile(req.m_path);

    (void)getFileModificationTime(req.m_path.c_str(),
                                  reply.m_fileModificationTime);

    reply.m_readOnly = sfu.isReadOnly(req.m_path);

  }
  PATH_REQUEST_CATCH_BLOCK

  return reply;
}


RFA_Msg_WriteFileReply VFS_LocalImpl::writeFile(
  RFA_Msg_WriteFileRequest const &req)
{
  SMFileUtil sfu;

  RFA_Msg_WriteFileReply reply;

  try {
    sfu.writeFile(req.m_path, req.m_contents);

    (void)getFileModificationTime(req.m_path.c_str(),
                                  reply.m_fileModificationTime);
  }
  PATH_REQUEST_CATCH_BLOCK

  return reply;
}


RFA_Msg_DeleteFileReply VFS_LocalImpl::deleteFile(
  RFA_Msg_DeleteFileRequest const &req)
{
  SMFileUtil sfu;

  RFA_Msg_DeleteFileReply reply;

  try {
    sfu.removeFile(req.m_path);
  }
  PATH_REQUEST_CATCH_BLOCK

  return reply;
}


RFA_Msg_GetDirEntriesReply VFS_LocalImpl::getDirEntries(
  RFA_Msg_GetDirEntriesRequest const &req)
{
  SMFileUtil sfu;

  RFA_Msg_GetDirEntriesReply reply;

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


RFA_Msg_MakeDirectoryReply VFS_LocalImpl::makeDirectory(
  RFA_Msg_MakeDirectoryRequest const &req)
{
  SMFileUtil sfu;

  RFA_Msg_MakeDirectoryReply reply;

  try {
    sfu.createDirectory(req.m_path);
  }
  PATH_REQUEST_CATCH_BLOCK

  return reply;
}


// EOF
