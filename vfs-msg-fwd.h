// vfs-msg-fwd.h
// Forwards for vfs-msg.h.

#ifndef EDITOR_VFS_MSG_FWD_H
#define EDITOR_VFS_MSG_FWD_H

// Abstract message classes.
class VFS_Message;
class VFS_PathRequest;
class VFS_PathReply;

// Do 'macro' for every kind of VFS message.
//
// This defines the list of message types.
#define FOR_EACH_VFS_MESSAGE_TYPE(macro) \
  macro(GetVersion)                      \
  macro(Echo)                            \
  macro(FileStatusRequest)               \
  macro(FileStatusReply)                 \
  macro(ReadFileRequest)                 \
  macro(ReadFileReply)                   \
  macro(WriteFileRequest)                \
  macro(WriteFileReply)                  \
  macro(DeleteFileRequest)               \
  macro(DeleteFileReply)                 \
  macro(GetDirEntriesRequest)            \
  macro(GetDirEntriesReply)              \
  /*nothing*/

#define FORWARD_DECLARE_VFS_CLASS(type) class VFS_##type;

FOR_EACH_VFS_MESSAGE_TYPE(FORWARD_DECLARE_VFS_CLASS)

#undef FORWARD_DECLARE_VFS_CLASS


#endif // EDITOR_VFS_MSG_FWD_H
