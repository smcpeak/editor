// vfs-local.h
// VFS_LocalImpl class.

// This module does not have any dependencies on Qt because it is meant
// to be the core of a file system server program that can be easily
// built for any target system.

#ifndef EDITOR_VFS_LOCAL_H
#define EDITOR_VFS_LOCAL_H

#include "rfa-msg.h"                   // request and reply messages


// Local implementation of virtual file system.
//
// This class synchronously turns requests into replies.
class VFS_LocalImpl {
public:      // methods
  RFA_FileStatusReply    queryPath    (RFA_FileStatusRequest    const &req);
  RFA_ReadFileReply      readFile     (RFA_ReadFileRequest      const &req);
  RFA_WriteFileReply     writeFile    (RFA_WriteFileRequest     const &req);
  RFA_DeleteFileReply    deleteFile   (RFA_DeleteFileRequest    const &req);
  RFA_GetDirEntriesReply getDirEntries(RFA_GetDirEntriesRequest const &req);
  RFA_MakeDirectoryReply makeDirectory(RFA_MakeDirectoryRequest const &req);
};


#endif // EDITOR_VFS_LOCAL_H
