// rfa-msg.h
// Definitions of message classes for a virtual file system.

// For now, I define all of these classes by hand, but envision at some
// point extending astgen so it can do this, including defining the
// serialization methods.

#ifndef EDITOR_RFA_MSG_H
#define EDITOR_RFA_MSG_H

#include "rfa-msg-fwd.h"                         // FOR_EACH_RFA_MESSAGE_TYPE, plus fwds for this module

// smbase
#include "smbase/flatten-fwd.h"                  // Flatten
#include "smbase/portable-error-code.h"          // smbase::PortableErrorCode
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/str.h"                          // string

// libc++
#include <vector>                                // std::vector

// libc
#include <stdint.h>                              // int64_t, int32_t


// Protocol version described in this file.
//
// The "protocol" refers to the set of message types, what fields they
// have and what those fields mean, and how they are serialized into
// octet sequences.  Its primary purpose is to cleanly detect
// incompatibilities between client and server.
//
// Version history:
//
//    1: First numbered version.
//    2: Add {Read,Write,Delete}File{Request,Reply}.
//    3: Make FileStatus{Request,Reply} inherit Path{Request,Reply}.
//    4: Add GetDirEntries{Request,Reply}.
//    5: Add RFA_PathReply::m_failureReasonCode.
//    6: Modify set of PortableErrorCodes.
//    7: Add MakeDirectory{Request,Reply}.
//
int32_t const RFA_currentVersion = 7;


// Possible kinds of RFA messages.
enum RFA_MessageType {
  #define MAKE_MT_ENUMERATOR(type) RFA_MT_##type,

  FOR_EACH_RFA_MESSAGE_TYPE(MAKE_MT_ENUMERATOR)

  #undef MAKE_MT_ENUMERATOR

  NUM_RFA_MESSAGE_TYPES
};

// Return a string like "GetVersion".
char const *toString(RFA_MessageType mt);


// Superclass for the message types.
class RFA_Message {
public:      // methods
  RFA_Message();
  virtual ~RFA_Message();

  // Which kind of message this is.
  virtual RFA_MessageType messageType() const = 0;

  // Description of the message, partly for debugging, but also used to
  // indicate what the application is waiting for in some cases.  The
  // default implementation is 'toString(messageType())'.
  virtual string description() const;

  // Serialize this object's message type into 'flat'.
  void serialize(Flatten &flat) const;

  // Deserialize the message in 'flat'.
  static RFA_Message *deserialize(Flatten &flat);

  // De/serialize derived class details.
  virtual void xfer(Flatten &flat) = 0;

  // Test for dynamic object type.
  #define RFA_DECLARE_DOWNCASTS(destType)                                                     \
    bool is##destType() const { return messageType() == RFA_MT_##destType; }                  \
    RFA_##destType const *as##destType##C() const;                                            \
    RFA_##destType *as##destType() { return const_cast<RFA_##destType*>(as##destType##C()); } \
    RFA_##destType const *if##destType##C() const;                                            \
    RFA_##destType *if##destType() { return const_cast<RFA_##destType*>(if##destType##C()); }

  FOR_EACH_RFA_MESSAGE_TYPE(RFA_DECLARE_DOWNCASTS)

  #undef RFA_DECLARE_DOWNCASTS
};


// Get the protocol version that the server understands.
//
// The request and reply are the same message type.
class RFA_GetVersion : public RFA_Message {
public:
  // In the request, this is the version the client understands.  In
  // the reply, it is what the server understands.
  int32_t m_version;

public:
  RFA_GetVersion();
  virtual ~RFA_GetVersion() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_GetVersion; }
  virtual void xfer(Flatten &flat) override;
};


// For testing the message interface, simply respond with the given
// string.  The request and reply are the same message type.
class RFA_Echo : public RFA_Message {
public:      // data
  // Data to be echoed.
  std::vector<unsigned char> m_data;

public:      // methods
  RFA_Echo();
  virtual ~RFA_Echo() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_Echo; }
  virtual void xfer(Flatten &flat) override;
};


// Abstract base class for requests applicable to a file system path.
class RFA_PathRequest : public RFA_Message {
public:      // data
  // Path to the file of interest.
  //
  // I'm still unsure if I want to insist that this be absolute.  Right
  // now, relative paths are accepted, and interpreted as relative to
  // wherever the server process happens to be started.
  string m_path;

public:      // methods
  RFA_PathRequest();
  virtual ~RFA_PathRequest() override;

  // RFA_Message methods.
  virtual string description() const override;
  virtual void xfer(Flatten &flat) override;
};


// Abstract base class for replies to a PathRequest.
class RFA_PathReply : public RFA_Message {
public:      // data
  // True if the operation completed successfully.  Initially true.
  bool m_success;

  // If '!m_success', the reason for the failure as a machine-readable
  // error code.  Initially PEC_NO_ERROR.
  smbase::PortableErrorCode m_failureReasonCode;

  // If '!m_success', the reason for the failure as a human-readable
  // string.  Initially empty.
  string m_failureReasonString;

public:      // methods
  RFA_PathReply();
  virtual ~RFA_PathReply() override;

  // Set the failure reason, and set 'm_success' to false.
  void setFailureReason(smbase::PortableErrorCode reasonCode,
                        string const &reasonString);

  // RFA_Message methods.
  virtual string description() const override;
  virtual void xfer(Flatten &flat) override;
};


// Query a file status: kind and timestamp.
class RFA_FileStatusRequest : public RFA_PathRequest {
public:      // methods
  RFA_FileStatusRequest();
  virtual ~RFA_FileStatusRequest() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_FileStatusRequest; }
};


// Reply for RFA_FileStatusRequest.
//
// If the path does not exist, then 'm_fileKind' is set to 'FK_NONE',
// and 'm_success' is true.  There is not currently a case where this
// reply carries a failure.
class RFA_FileStatusReply : public RFA_PathReply {
public:      // data
  // Absolute directory containing 'm_path'.  This always ends with a
  // directory separator.
  string m_dirName;

  // Final file name component of 'm_path'.
  string m_fileName;

  // True if the 'm_dirName' exists and is a directory.
  bool m_dirExists;

  // Existence and kind of 'm_fileName'.
  SMFileUtil::FileKind m_fileKind;

  // If 'm_fileName' exists, its unix modification time.
  int64_t m_fileModificationTime;

public:      // methods
  RFA_FileStatusReply();
  virtual ~RFA_FileStatusReply() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_FileStatusReply; }
  virtual string description() const override;
  virtual void xfer(Flatten &flat) override;
};


// Request to read the contents of a file.
class RFA_ReadFileRequest : public RFA_PathRequest {
public:      // methods
  RFA_ReadFileRequest();
  virtual ~RFA_ReadFileRequest() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_ReadFileRequest; }
};


// Reply with contents of a file.
class RFA_ReadFileReply : public RFA_PathReply {
public:      // data
  // File contents.
  std::vector<unsigned char> m_contents;

  // Modification time as reported by the file system.
  int64_t m_fileModificationTime;

  // True if the file is marked read-only.
  bool m_readOnly;

public:      // methods
  RFA_ReadFileReply();
  virtual ~RFA_ReadFileReply() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_ReadFileReply; }
  virtual string description() const override;
  virtual void xfer(Flatten &flat) override;
};


// Request to write the contents of a file.
class RFA_WriteFileRequest : public RFA_PathRequest {
public:      // data
  // File contents to write.
  std::vector<unsigned char> m_contents;

public:      // methods
  RFA_WriteFileRequest();
  virtual ~RFA_WriteFileRequest() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_WriteFileRequest; }
  virtual void xfer(Flatten &flat) override;
};


// Reply to RFA_WriteFileRequest.
class RFA_WriteFileReply : public RFA_PathReply {
public:      // data
  // Modification time as reported by the file system *after* writing
  // the file's contents.
  int64_t m_fileModificationTime;

public:      // methods
  RFA_WriteFileReply();
  virtual ~RFA_WriteFileReply() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_WriteFileReply; }
  virtual void xfer(Flatten &flat) override;
};


// Request to delete a file.
class RFA_DeleteFileRequest : public RFA_PathRequest {
public:      // methods
  RFA_DeleteFileRequest();
  virtual ~RFA_DeleteFileRequest() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_DeleteFileRequest; }
};


// Reply to RFA_DeleteFileRequest.
class RFA_DeleteFileReply : public RFA_PathReply {
public:      // methods
  RFA_DeleteFileReply();
  virtual ~RFA_DeleteFileReply() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_DeleteFileReply; }
};


// Get the contents of 'm_path' as a directory.
class RFA_GetDirEntriesRequest : public RFA_PathRequest {
public:      // methods
  RFA_GetDirEntriesRequest();
  virtual ~RFA_GetDirEntriesRequest() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_GetDirEntriesRequest; }
};


// Reply to RFA_GetDirEntriesRequest.
class RFA_GetDirEntriesReply : public RFA_PathReply {
public:      // data
  // Entries of 'm_path', sorted by name.
  std::vector<SMFileUtil::DirEntryInfo> m_entries;

public:      // methods
  RFA_GetDirEntriesReply();
  virtual ~RFA_GetDirEntriesReply() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_GetDirEntriesReply; }
  virtual void xfer(Flatten &flat) override;
};


// Request to make a directory.
//
// This only creates one level of directory; `m_path` must be of the
// form "$DIR/$NAME", where $DIR exists and $NAME does not.
//
class RFA_MakeDirectoryRequest : public RFA_PathRequest {
public:      // methods
  RFA_MakeDirectoryRequest();
  virtual ~RFA_MakeDirectoryRequest() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_MakeDirectoryRequest; }
};


// Reply to RFA_MakeDirectoryRequest.
class RFA_MakeDirectoryReply : public RFA_PathReply {
public:      // methods
  RFA_MakeDirectoryReply();
  virtual ~RFA_MakeDirectoryReply() override;

  // RFA_Message methods.
  virtual RFA_MessageType messageType() const override
    { return RFA_MT_MakeDirectoryReply; }
};


#endif // EDITOR_RFA_MSG_H
