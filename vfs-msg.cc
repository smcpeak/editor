// vfs-msg.cc
// Code for vfs-msg.h.

#include "vfs-msg.h"                   // this module

// smbase
#include "flatutil.h"                  // xferEnum, Flatten, xferVectorBytewise


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

  // Create an object of the corresponding type.
  switch (mtype) {
    default:
      xformat(stringb("Invalid message type: " << mtype));

    #define HANDLE_TYPE(type)   \
      case VFS_MT_##type:       \
        ret = new VFS_##type(); \
        break;

    HANDLE_TYPE(GetVersion)
    HANDLE_TYPE(Echo)
    HANDLE_TYPE(PathRequest)
    HANDLE_TYPE(PathReply)

    #undef HANDLE_TYPE
  }

  // Ensure the created object agrees about the type.
  xassert(ret->messageType() == mtype);

  // Read details.
  ret->xfer(flat);

  return ret;
}


#define VFS_DEFINE_DOWNCASTS(destType)                       \
  VFS_##destType const *VFS_Message::as##destType##C() const \
  {                                                          \
    xassert(is##destType());                                 \
    return static_cast<VFS_##destType const*>(this);         \
  }                                                          \
  VFS_##destType const *VFS_Message::if##destType##C() const \
  {                                                          \
    if (!is##destType()) {                                   \
      return nullptr;                                        \
    }                                                        \
    return static_cast<VFS_##destType const*>(this);         \
  }

VFS_DEFINE_DOWNCASTS(GetVersion)
VFS_DEFINE_DOWNCASTS(Echo)
VFS_DEFINE_DOWNCASTS(PathRequest)
VFS_DEFINE_DOWNCASTS(PathReply)

#undef VFS_DEFINE_DOWNCASTS


// ------------------------- VFS_GetVersion ----------------------------
VFS_GetVersion::VFS_GetVersion()
  : VFS_Message(),
    m_version(VFS_currentVersion)
{}


VFS_GetVersion::~VFS_GetVersion()
{}


void VFS_GetVersion::xfer(Flatten &flat)
{
  flat.xfer_int32_t(m_version);
}


// ---------------------------- VFS_Echo -------------------------------
VFS_Echo::VFS_Echo()
  : VFS_Message(),
    m_data()
{}


VFS_Echo::~VFS_Echo()
{}


void VFS_Echo::xfer(Flatten &flat)
{
  xferVectorBytewise(flat, m_data);
}


// ------------------------- VFS_PathRequest ---------------------------
VFS_PathRequest::VFS_PathRequest()
  : VFS_Message(),
    m_path()
{}


VFS_PathRequest::~VFS_PathRequest()
{}


void VFS_PathRequest::xfer(Flatten &flat)
{
  m_path.xfer(flat);
}


// -------------------------- VFS_PathReply ----------------------------
VFS_PathReply::VFS_PathReply()
  : VFS_Message(),
    m_dirName(),
    m_fileName(),
    m_dirExists(false),
    m_fileKind(SMFileUtil::FK_NONE),
    m_fileModificationTime(0)
{}


VFS_PathReply::~VFS_PathReply()
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
