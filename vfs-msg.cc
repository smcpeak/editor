// vfs-msg.cc
// Code for vfs-msg.h.

#include "vfs-msg.h"                   // this module

// smbase
#include "flatutil.h"                  // xferEnum, Flatten


// --------------------------- VFS_Message -----------------------------
VFS_Message::VFS_Message()
{}


VFS_Message::~VFS_Message()
{}


void VFS_Message::serialize(Flatten &flat) const
{
  xassert(flat.writing());

  // Write the message type.
  VFS_MessageType mtype = messageType();
  xferEnum(flat, mtype);

  // Then the derived class details.
  //
  // We have to cast away constness, but that should be safe because
  // 'xfer' should never modify this object when 'flat' is writing.
  const_cast<VFS_Message*>(this)->xfer(flat);
}


/*static*/ VFS_Message *VFS_Message::deserialize(Flatten &flat)
{
  xassert(flat.reading());

  // Read message type.
  VFS_MessageType mtype;
  xferEnum(flat, mtype);

  VFS_Message *ret = nullptr;

  switch (mtype) {
    default:
      xformat(stringb("Invalid message type: " << mtype));

    case VFS_MT_PathRequest:
      ret = new VFS_PathRequest(flat);
      break;

    case VFS_MT_PathReply:
      ret = new VFS_PathReply(flat);
      break;
  }

  // Read details.
  ret->xfer(flat);

  return ret;
}


// ------------------------- VFS_PathRequest ---------------------------
VFS_PathRequest::VFS_PathRequest(string path)
  : m_path(path)
{}


VFS_PathRequest::~VFS_PathRequest()
{}


VFS_PathRequest::VFS_PathRequest(Flatten&)
  : m_path()
{}


VFS_MessageType VFS_PathRequest::messageType() const
{
  return VFS_MT_PathRequest;
}


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


VFS_MessageType VFS_PathReply::messageType() const
{
  return VFS_MT_PathReply;
}


void VFS_PathReply::xfer(Flatten &flat)
{
  m_dirName.xfer(flat);
  m_fileName.xfer(flat);
  flat.xferBool(m_dirExists);
  xferEnum(flat, m_fileKind);
  flat.xfer_int64_t(m_fileModificationTime);
}


// EOF
