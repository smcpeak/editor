// vfs-msg.h
// Definitions of message classes for a virtual file system.

// For now, I define all of these classes by hand, but envision at some
// point extending astgen so it can do this, including defining the
// serialization methods.

#ifndef EDITOR_VFS_MSG_H
#define EDITOR_VFS_MSG_H

// smbase
#include "flatten-fwd.h"               // Flatten
#include "sm-file-util.h"              // SMFileUtil
#include "str.h"                       // string

// libc
#include <stdint.h>                    // int64_t


// Query a path name.
class VFS_PathRequest {
public:      // data
  // File path to query.
  //
  // I'm expecting this to usually be an absolute path, but I might
  // handle relative paths too, in which case they are relative to
  // wherever the server process is started from.
  string m_path;

public:      // methods
  VFS_PathRequest(string path);
  ~VFS_PathRequest();

  VFS_PathRequest(VFS_PathRequest const &obj) = default;
  VFS_PathRequest& operator=(VFS_PathRequest const &obj) = default;

  VFS_PathRequest(Flatten&);
  void xfer(Flatten &flat);
};


// Reply for VFS_PathRequest.
class VFS_PathReply {
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
  ~VFS_PathReply();

  VFS_PathReply(VFS_PathReply const &obj) = default;
  VFS_PathReply& operator=(VFS_PathReply const &obj) = default;

  VFS_PathReply(Flatten&);
  void xfer(Flatten &flat);
};


#endif // EDITOR_VFS_MSG_H
