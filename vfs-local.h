// vfs-local.h
// VFS_LocalImpl class.

// This module does not have any dependencies on Qt because it is meant
// to be the core of a file system server program that can be easily
// built for any target system.

#ifndef EDITOR_VFS_LOCAL_H
#define EDITOR_VFS_LOCAL_H

#include "vfs-msg.h"                   // request and reply messages


// Local implementation of virtual file system.
//
// This class synchronously turns requests into replies.
class VFS_LocalImpl {
public:      // methods
  VFS_PathReply       queryPath (VFS_PathRequest       const &req);
  VFS_ReadFileReply   readFile  (VFS_ReadFileRequest   const &req);
  VFS_WriteFileReply  writeFile (VFS_WriteFileRequest  const &req);
  VFS_DeleteFileReply deleteFile(VFS_DeleteFileRequest const &req);
};


#endif // EDITOR_VFS_LOCAL_H
