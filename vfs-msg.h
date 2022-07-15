// vfs-msg.h
// Definitions of message classes for a virtual file system.

// For now, I define all of these classes by hand, but envision at some
// point extending astgen so it can do this, including defining the
// serialization methods.

#ifndef EDITOR_VFS_MSG_H
#define EDITOR_VFS_MSG_H

#include "vfs-msg-fwd.h"               // fwds for this module

// smbase
#include "flatten-fwd.h"               // Flatten
#include "sm-file-util.h"              // SMFileUtil
#include "str.h"                       // string

// libc++
#include <vector>                      // std::vector

// libc
#include <stdint.h>                    // int64_t, int32_t


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
//
int32_t const VFS_currentVersion = 1;


// Possible kinds of VFS messages.
enum VFS_MessageType {
  VFS_MT_GetVersion,
  VFS_MT_Echo,

  VFS_MT_PathRequest,
  VFS_MT_PathReply,
};


// Superclass for the message types.
class VFS_Message {
public:      // methods
  VFS_Message();
  virtual ~VFS_Message();

  // Which kind of message this is.
  virtual VFS_MessageType messageType() const = 0;

  // Serialize this object's message type into 'flat'.
  void serialize(Flatten &flat) const;

  // Deserialize the message in 'flat'.
  static VFS_Message *deserialize(Flatten &flat);

  // De/serialize derived class details.
  virtual void xfer(Flatten &flat) = 0;

  // Test for dynamic object type.
  #define VFS_DECLARE_DOWNCASTS(destType)                                                     \
    bool is##destType() const { return messageType() == VFS_MT_##destType; }                  \
    VFS_##destType const *as##destType##C() const;                                            \
    VFS_##destType *as##destType() { return const_cast<VFS_##destType*>(as##destType##C()); } \
    VFS_##destType const *if##destType##C() const;                                            \
    VFS_##destType *if##destType() { return const_cast<VFS_##destType*>(if##destType##C()); }

  VFS_DECLARE_DOWNCASTS(GetVersion)
  VFS_DECLARE_DOWNCASTS(Echo)
  VFS_DECLARE_DOWNCASTS(PathRequest)
  VFS_DECLARE_DOWNCASTS(PathReply)

  #undef VFS_DECLARE_DOWNCASTS
};


// Get the protocol version that the server understands.
//
// The request and reply are the same message type.
class VFS_GetVersion : public VFS_Message {
public:
  // In the request, this is the version the client understands.  In
  // the reply, it is what the server understands.
  int32_t m_version;

public:
  VFS_GetVersion();
  virtual ~VFS_GetVersion() override;

  // VFS_Message methods.
  virtual VFS_MessageType messageType() const override
    { return VFS_MT_GetVersion; }
  virtual void xfer(Flatten &flat) override;
};


// For testing the message interface, simply respond with the given
// string.  The request and reply are the same message type.
class VFS_Echo : public VFS_Message {
public:      // data
  // Data to be echoed.
  std::vector<unsigned char> m_data;

public:      // methods
  VFS_Echo();
  virtual ~VFS_Echo() override;

  // VFS_Message methods.
  virtual VFS_MessageType messageType() const override
    { return VFS_MT_Echo; }
  virtual void xfer(Flatten &flat) override;
};


// Query a path name.
class VFS_PathRequest : public VFS_Message {
public:      // data
  // File path to query.
  //
  // I'm expecting this to usually be an absolute path, but I might
  // handle relative paths too, in which case they are relative to
  // wherever the server process is started from.
  string m_path;

public:      // methods
  VFS_PathRequest();
  virtual ~VFS_PathRequest() override;

  // VFS_Message methods.
  virtual VFS_MessageType messageType() const override
    { return VFS_MT_PathRequest; }
  virtual void xfer(Flatten &flat) override;
};


// Reply for VFS_PathRequest.
class VFS_PathReply : public VFS_Message {
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
  VFS_PathReply();
  virtual ~VFS_PathReply() override;

  // VFS_Message methods.
  virtual VFS_MessageType messageType() const override
    { return VFS_MT_PathReply; }
  virtual void xfer(Flatten &flat) override;
};


#endif // EDITOR_VFS_MSG_H
