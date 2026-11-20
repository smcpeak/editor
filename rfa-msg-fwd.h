// rfa-msg-fwd.h
// Forwards for rfa-msg.h.

#ifndef EDITOR_RFA_MSG_FWD_H
#define EDITOR_RFA_MSG_FWD_H

// Abstract message classes.
class RFA_Message;
class RFA_PathRequest;
class RFA_PathReply;

// Do 'macro' for every kind of RFA message.
//
// This defines the list of message types.
#define FOR_EACH_RFA_MESSAGE_TYPE(macro) \
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
  macro(MakeDirectoryRequest)            \
  macro(MakeDirectoryReply)              \
  /*nothing*/

// TODO: Rename the message classes as RFA_Msg_*.
#define FORWARD_DECLARE_RFA_CLASS(type) class RFA_##type;

FOR_EACH_RFA_MESSAGE_TYPE(FORWARD_DECLARE_RFA_CLASS)

#undef FORWARD_DECLARE_RFA_CLASS


#endif // EDITOR_RFA_MSG_FWD_H
