// vfs-msg.cc
// Code for vfs-msg.h.

#include "vfs-msg.h"                   // this module

// smbase
#include "flatutil.h"                  // xferEnum, Flatten


// ------------------------- VFS_PathRequest ---------------------------
VFS_PathRequest::VFS_PathRequest(string path)
  : m_path(path)
{}


VFS_PathRequest::~VFS_PathRequest()
{}


VFS_PathRequest::VFS_PathRequest(Flatten&)
  : m_path()
{}


void VFS_PathRequest::xfer(Flatten &flat)
{
  m_path.xfer(flat);
}


// -------------------------- VFS_PathReply ----------------------------
VFS_PathReply::VFS_PathReply()
  : m_dirName(),
    m_fileName(),
    m_dirExists(false),
    m_fileKind(SMFileUtil::FK_NONE),
    m_fileModificationTime(0)
{}


VFS_PathReply::~VFS_PathReply()
{}


VFS_PathReply::VFS_PathReply(Flatten&)
  : VFS_PathReply()
{}


void VFS_PathReply::xfer(Flatten &flat)
{
  m_dirName.xfer(flat);
  m_fileName.xfer(flat);
  flat.xferBool(m_dirExists);
  xferEnum(flat, m_fileKind);
  flat.xfer_int64_t(m_fileModificationTime);
}


// EOF
