// rfa-sync-local-client.h
// RFA_SyncLocalClient class.

// See license.txt for copyright and terms of use.

// This module does not have any dependencies on Qt because it is meant
// to be the core of a file system server program that can be easily
// built for any target system.

#ifndef EDITOR_RFA_SYNC_LOCAL_CLIENT_H
#define EDITOR_RFA_SYNC_LOCAL_CLIENT_H

#include "rfa-msg.h"                   // request and reply messages


// Local implementation of virtual file system.
//
// This class synchronously turns requests into replies.
//
// TODO: Have this implement RFA_AbstractSyncMultiClient once that
// exists.
class RFA_SyncLocalClient {
public:      // methods
  RFA_Msg_FileStatusReply    queryPath    (RFA_Msg_FileStatusRequest    const &req);
  RFA_Msg_ReadFileReply      readFile     (RFA_Msg_ReadFileRequest      const &req);
  RFA_Msg_WriteFileReply     writeFile    (RFA_Msg_WriteFileRequest     const &req);
  RFA_Msg_DeleteFileReply    deleteFile   (RFA_Msg_DeleteFileRequest    const &req);
  RFA_Msg_GetDirEntriesReply getDirEntries(RFA_Msg_GetDirEntriesRequest const &req);
  RFA_Msg_MakeDirectoryReply makeDirectory(RFA_Msg_MakeDirectoryRequest const &req);
};


#endif // EDITOR_RFA_SYNC_LOCAL_CLIENT_H
