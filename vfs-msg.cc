// vfs-msg.cc
// Code for vfs-msg.h.

#include "vfs-msg.h"                   // this module

// smbase
#include "smbase/exc.h"                // xformatdb, smbase::XSysError
#include "smbase/flatutil.h"           // xferEnum, Flatten, xferVectorBytewise

using namespace smbase;


#define STRINGIZE_VFS_MT(type) #type,

DEFINE_ENUMERATION_TO_STRING(VFS_MessageType, NUM_VFS_MESSAGE_TYPES,
  ( FOR_EACH_VFS_MESSAGE_TYPE(STRINGIZE_VFS_MT) ))

#undef STRINGIZE_VFS_MT


// --------------------------- VFS_Message -----------------------------
VFS_Message::VFS_Message()
{}


VFS_Message::~VFS_Message()
{}


string VFS_Message::description() const
{
  return toString(messageType());
}


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

  static_assert(NUM_VFS_MESSAGE_TYPES == 14,
    "Bump protocol version when number of message types changes.");

  // Read message type.
  VFS_MessageType mtype;
  xferEnum(flat, mtype);

  VFS_Message *ret = nullptr;

  // Create an object of the corresponding type.
  switch (mtype) {
    default:
      xformatsb("Invalid message type: " << mtype);

    #define HANDLE_TYPE(type)   \
      case VFS_MT_##type:       \
        ret = new VFS_##type(); \
        break;

    FOR_EACH_VFS_MESSAGE_TYPE(HANDLE_TYPE)

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

FOR_EACH_VFS_MESSAGE_TYPE(VFS_DEFINE_DOWNCASTS)

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


// ---------------------- VFS_FileStatusRequest ------------------------
VFS_FileStatusRequest::VFS_FileStatusRequest()
  : VFS_PathRequest()
{}


VFS_FileStatusRequest::~VFS_FileStatusRequest()
{}


// ----------------------- VFS_FileStatusReply -------------------------
VFS_FileStatusReply::VFS_FileStatusReply()
  : VFS_PathReply(),
    m_dirName(),
    m_fileName(),
    m_dirExists(false),
    m_fileKind(SMFileUtil::FK_NONE),
    m_fileModificationTime(0)
{}


VFS_FileStatusReply::~VFS_FileStatusReply()
{}


string VFS_FileStatusReply::description() const
{
  std::ostringstream sb;
  sb << VFS_PathReply::description();
  sb << " dirName=\"" << m_dirName
     << "\" fileName=\"" << m_fileName
     << "\" dirExists=" << m_dirExists
     << " fileKind=" << toString(m_fileKind)
     << " modTime=" << m_fileModificationTime
     ;
  return sb.str();
}


void VFS_FileStatusReply::xfer(Flatten &flat)
{
  VFS_PathReply::xfer(flat);

  stringXfer(m_dirName, flat);
  stringXfer(m_fileName, flat);
  flat.xferBool(m_dirExists);
  xferEnum(flat, m_fileKind);
  flat.xfer_int64_t(m_fileModificationTime);
}


// ------------------------- VFS_PathRequest ---------------------------
VFS_PathRequest::VFS_PathRequest()
  : VFS_Message(),
    m_path()
{}


VFS_PathRequest::~VFS_PathRequest()
{}


string VFS_PathRequest::description() const
{
  return stringb(toString(messageType()) << " for \"" << m_path << "\"");
}


void VFS_PathRequest::xfer(Flatten &flat)
{
  stringXfer(m_path, flat);
}


// -------------------------- VFS_PathReply ----------------------------
VFS_PathReply::VFS_PathReply()
  : VFS_Message(),
    m_success(true),
    m_failureReasonCode(PortableErrorCode::PEC_NO_ERROR),
    m_failureReasonString()
{}


VFS_PathReply::~VFS_PathReply()
{}


void VFS_PathReply::setFailureReason(PortableErrorCode reasonCode,
                                     string const &reasonString)
{
  m_success = false;
  m_failureReasonCode = reasonCode;
  m_failureReasonString = reasonString;

  static_assert(int(PortableErrorCode::NUM_REASONS) == 13,
    "Must bump VFS version number if set of reason codes change.");
}


string VFS_PathReply::description() const
{
  std::ostringstream sb;
  sb << toString(messageType()) << ": ";
  if (m_success) {
    sb << "success";
  }
  else {
    sb << "failure=\"" << m_failureReasonString << "\"";
  }
  return sb.str();
}


void VFS_PathReply::xfer(Flatten &flat)
{
  flat.xferBool(m_success);
  xferEnum(flat, m_failureReasonCode);
  stringXfer(m_failureReasonString, flat);
}


// ----------------------- VFS_ReadFileRequest -------------------------
VFS_ReadFileRequest::VFS_ReadFileRequest()
  : VFS_PathRequest()
{}


VFS_ReadFileRequest::~VFS_ReadFileRequest()
{}


// ------------------------ VFS_ReadFileReply --------------------------
VFS_ReadFileReply::VFS_ReadFileReply()
  : VFS_PathReply(),
    m_contents(),
    m_fileModificationTime(0),
    m_readOnly(false)
{}


VFS_ReadFileReply::~VFS_ReadFileReply()
{}


string VFS_ReadFileReply::description() const
{
  std::ostringstream sb;
  sb << VFS_PathReply::description()
     << " size=" << m_contents.size()
     << " modTime=" << m_fileModificationTime
     << " readOnly=" << m_readOnly;
  return sb.str();
}


void VFS_ReadFileReply::xfer(Flatten &flat)
{
  VFS_PathReply::xfer(flat);

  xferVectorBytewise(flat, m_contents);
  flat.xfer_int64_t(m_fileModificationTime);
  flat.xferBool(m_readOnly);
}


// ----------------------- VFS_WriteFileRequest ------------------------
VFS_WriteFileRequest::VFS_WriteFileRequest()
  : VFS_PathRequest(),
    m_contents()
{}


VFS_WriteFileRequest::~VFS_WriteFileRequest()
{}


void VFS_WriteFileRequest::xfer(Flatten &flat)
{
  VFS_PathRequest::xfer(flat);

  xferVectorBytewise(flat, m_contents);
}


// ------------------------ VFS_WriteFileReply -------------------------
VFS_WriteFileReply::VFS_WriteFileReply()
  : VFS_PathReply(),
    m_fileModificationTime(0)
{}


VFS_WriteFileReply::~VFS_WriteFileReply()
{}


void VFS_WriteFileReply::xfer(Flatten &flat)
{
  VFS_PathReply::xfer(flat);

  flat.xfer_int64_t(m_fileModificationTime);
}


// ---------------------- VFS_DeleteFileRequest ------------------------
VFS_DeleteFileRequest::VFS_DeleteFileRequest()
  : VFS_PathRequest()
{}


VFS_DeleteFileRequest::~VFS_DeleteFileRequest()
{}


// ----------------------- VFS_DeleteFileReply -------------------------
VFS_DeleteFileReply::VFS_DeleteFileReply()
  : VFS_PathReply()
{}


VFS_DeleteFileReply::~VFS_DeleteFileReply()
{}


// ---------------------- VFS_GetDirEnriesRequest ----------------------
VFS_GetDirEntriesRequest::VFS_GetDirEntriesRequest()
  : VFS_PathRequest()
{}


VFS_GetDirEntriesRequest::~VFS_GetDirEntriesRequest()
{}


// ---------------------- VFS_GetDirEntriesReply -----------------------
VFS_GetDirEntriesReply::VFS_GetDirEntriesReply()
  : VFS_PathReply()
{}


VFS_GetDirEntriesReply::~VFS_GetDirEntriesReply()
{}


[[maybe_unused]]   // clangd is confused, this is used.
static void xfer(Flatten &flat, SMFileUtil::DirEntryInfo &info)
{
  stringXfer(info.m_name, flat);
  xferEnum(flat, info.m_kind);

  static_assert(SMFileUtil::NUM_FILE_KINDS == 4,
    "The FileKind enumeration has changed, requiring a bump to the "
    "protocol version.");
}


void VFS_GetDirEntriesReply::xfer(Flatten &flat)
{
  VFS_PathReply::xfer(flat);

  ::xfer(flat, m_entries);
}


// --------------------- VFS_MakeDirectoryRequest ----------------------
VFS_MakeDirectoryRequest::VFS_MakeDirectoryRequest()
  : VFS_PathRequest()
{}


VFS_MakeDirectoryRequest::~VFS_MakeDirectoryRequest()
{}


// ---------------------- VFS_MakeDirectoryReply -----------------------
VFS_MakeDirectoryReply::VFS_MakeDirectoryReply()
  : VFS_PathReply()
{}


VFS_MakeDirectoryReply::~VFS_MakeDirectoryReply()
{}


// EOF
