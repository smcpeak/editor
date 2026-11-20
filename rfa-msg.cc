// rfa-msg.cc
// Code for rfa-msg.h.

#include "rfa-msg.h"                   // this module

// smbase
#include "smbase/exc.h"                // xformatdb, smbase::XSysError
#include "smbase/flatutil.h"           // xferEnum, Flatten, xferVectorBytewise

using namespace smbase;


#define STRINGIZE_RFA_MT(type) #type,

DEFINE_ENUMERATION_TO_STRING(RFA_MessageType, NUM_RFA_MESSAGE_TYPES,
  ( FOR_EACH_RFA_MESSAGE_TYPE(STRINGIZE_RFA_MT) ))

#undef STRINGIZE_RFA_MT


// --------------------------- RFA_Message -----------------------------
RFA_Message::RFA_Message()
{}


RFA_Message::~RFA_Message()
{}


string RFA_Message::description() const
{
  return toString(messageType());
}


void RFA_Message::serialize(Flatten &flat) const
{
  xassert(flat.writing());

  // Write the message type.
  RFA_MessageType mtype = messageType();
  xferEnum(flat, mtype);

  // Then the derived class details.
  //
  // We have to cast away constness, but that should be safe because
  // 'xfer' should never modify this object when 'flat' is writing.
  const_cast<RFA_Message*>(this)->xfer(flat);
}


/*static*/ RFA_Message *RFA_Message::deserialize(Flatten &flat)
{
  xassert(flat.reading());

  static_assert(NUM_RFA_MESSAGE_TYPES == 14,
    "Bump protocol version when number of message types changes.");

  // Read message type.
  RFA_MessageType mtype;
  xferEnum(flat, mtype);

  RFA_Message *ret = nullptr;

  // Create an object of the corresponding type.
  switch (mtype) {
    default:
      xformatsb("Invalid message type: " << mtype);

    #define HANDLE_TYPE(type)   \
      case RFA_MT_##type:       \
        ret = new RFA_##type(); \
        break;

    FOR_EACH_RFA_MESSAGE_TYPE(HANDLE_TYPE)

    #undef HANDLE_TYPE
  }

  // Ensure the created object agrees about the type.
  xassert(ret->messageType() == mtype);

  // Read details.
  ret->xfer(flat);

  return ret;
}


#define RFA_DEFINE_DOWNCASTS(destType)                       \
  RFA_##destType const *RFA_Message::as##destType##C() const \
  {                                                          \
    xassert(is##destType());                                 \
    return static_cast<RFA_##destType const*>(this);         \
  }                                                          \
  RFA_##destType const *RFA_Message::if##destType##C() const \
  {                                                          \
    if (!is##destType()) {                                   \
      return nullptr;                                        \
    }                                                        \
    return static_cast<RFA_##destType const*>(this);         \
  }

FOR_EACH_RFA_MESSAGE_TYPE(RFA_DEFINE_DOWNCASTS)

#undef RFA_DEFINE_DOWNCASTS


// ------------------------- RFA_GetVersion ----------------------------
RFA_GetVersion::RFA_GetVersion()
  : RFA_Message(),
    m_version(RFA_currentVersion)
{}


RFA_GetVersion::~RFA_GetVersion()
{}


void RFA_GetVersion::xfer(Flatten &flat)
{
  flat.xfer_int32_t(m_version);
}


// ---------------------------- RFA_Echo -------------------------------
RFA_Echo::RFA_Echo()
  : RFA_Message(),
    m_data()
{}


RFA_Echo::~RFA_Echo()
{}


void RFA_Echo::xfer(Flatten &flat)
{
  xferVectorBytewise(flat, m_data);
}


// ---------------------- RFA_FileStatusRequest ------------------------
RFA_FileStatusRequest::RFA_FileStatusRequest()
  : RFA_PathRequest()
{}


RFA_FileStatusRequest::~RFA_FileStatusRequest()
{}


// ----------------------- RFA_FileStatusReply -------------------------
RFA_FileStatusReply::RFA_FileStatusReply()
  : RFA_PathReply(),
    m_dirName(),
    m_fileName(),
    m_dirExists(false),
    m_fileKind(SMFileUtil::FK_NONE),
    m_fileModificationTime(0)
{}


RFA_FileStatusReply::~RFA_FileStatusReply()
{}


string RFA_FileStatusReply::description() const
{
  std::ostringstream sb;
  sb << RFA_PathReply::description();
  sb << " dirName=\"" << m_dirName
     << "\" fileName=\"" << m_fileName
     << "\" dirExists=" << m_dirExists
     << " fileKind=" << toString(m_fileKind)
     << " modTime=" << m_fileModificationTime
     ;
  return sb.str();
}


void RFA_FileStatusReply::xfer(Flatten &flat)
{
  RFA_PathReply::xfer(flat);

  stringXfer(m_dirName, flat);
  stringXfer(m_fileName, flat);
  flat.xferBool(m_dirExists);
  xferEnum(flat, m_fileKind);
  flat.xfer_int64_t(m_fileModificationTime);
}


// ------------------------- RFA_PathRequest ---------------------------
RFA_PathRequest::RFA_PathRequest()
  : RFA_Message(),
    m_path()
{}


RFA_PathRequest::~RFA_PathRequest()
{}


string RFA_PathRequest::description() const
{
  return stringb(toString(messageType()) << " for \"" << m_path << "\"");
}


void RFA_PathRequest::xfer(Flatten &flat)
{
  stringXfer(m_path, flat);
}


// -------------------------- RFA_PathReply ----------------------------
RFA_PathReply::RFA_PathReply()
  : RFA_Message(),
    m_success(true),
    m_failureReasonCode(PortableErrorCode::PEC_NO_ERROR),
    m_failureReasonString()
{}


RFA_PathReply::~RFA_PathReply()
{}


void RFA_PathReply::setFailureReason(PortableErrorCode reasonCode,
                                     string const &reasonString)
{
  m_success = false;
  m_failureReasonCode = reasonCode;
  m_failureReasonString = reasonString;

  static_assert(int(PortableErrorCode::NUM_REASONS) == 13,
    "Must bump RFA version number if set of reason codes change.");
}


string RFA_PathReply::description() const
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


void RFA_PathReply::xfer(Flatten &flat)
{
  flat.xferBool(m_success);
  xferEnum(flat, m_failureReasonCode);
  stringXfer(m_failureReasonString, flat);
}


// ----------------------- RFA_ReadFileRequest -------------------------
RFA_ReadFileRequest::RFA_ReadFileRequest()
  : RFA_PathRequest()
{}


RFA_ReadFileRequest::~RFA_ReadFileRequest()
{}


// ------------------------ RFA_ReadFileReply --------------------------
RFA_ReadFileReply::RFA_ReadFileReply()
  : RFA_PathReply(),
    m_contents(),
    m_fileModificationTime(0),
    m_readOnly(false)
{}


RFA_ReadFileReply::~RFA_ReadFileReply()
{}


string RFA_ReadFileReply::description() const
{
  std::ostringstream sb;
  sb << RFA_PathReply::description()
     << " size=" << m_contents.size()
     << " modTime=" << m_fileModificationTime
     << " readOnly=" << m_readOnly;
  return sb.str();
}


void RFA_ReadFileReply::xfer(Flatten &flat)
{
  RFA_PathReply::xfer(flat);

  xferVectorBytewise(flat, m_contents);
  flat.xfer_int64_t(m_fileModificationTime);
  flat.xferBool(m_readOnly);
}


// ----------------------- RFA_WriteFileRequest ------------------------
RFA_WriteFileRequest::RFA_WriteFileRequest()
  : RFA_PathRequest(),
    m_contents()
{}


RFA_WriteFileRequest::~RFA_WriteFileRequest()
{}


void RFA_WriteFileRequest::xfer(Flatten &flat)
{
  RFA_PathRequest::xfer(flat);

  xferVectorBytewise(flat, m_contents);
}


// ------------------------ RFA_WriteFileReply -------------------------
RFA_WriteFileReply::RFA_WriteFileReply()
  : RFA_PathReply(),
    m_fileModificationTime(0)
{}


RFA_WriteFileReply::~RFA_WriteFileReply()
{}


void RFA_WriteFileReply::xfer(Flatten &flat)
{
  RFA_PathReply::xfer(flat);

  flat.xfer_int64_t(m_fileModificationTime);
}


// ---------------------- RFA_DeleteFileRequest ------------------------
RFA_DeleteFileRequest::RFA_DeleteFileRequest()
  : RFA_PathRequest()
{}


RFA_DeleteFileRequest::~RFA_DeleteFileRequest()
{}


// ----------------------- RFA_DeleteFileReply -------------------------
RFA_DeleteFileReply::RFA_DeleteFileReply()
  : RFA_PathReply()
{}


RFA_DeleteFileReply::~RFA_DeleteFileReply()
{}


// ---------------------- RFA_GetDirEnriesRequest ----------------------
RFA_GetDirEntriesRequest::RFA_GetDirEntriesRequest()
  : RFA_PathRequest()
{}


RFA_GetDirEntriesRequest::~RFA_GetDirEntriesRequest()
{}


// ---------------------- RFA_GetDirEntriesReply -----------------------
RFA_GetDirEntriesReply::RFA_GetDirEntriesReply()
  : RFA_PathReply()
{}


RFA_GetDirEntriesReply::~RFA_GetDirEntriesReply()
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


void RFA_GetDirEntriesReply::xfer(Flatten &flat)
{
  RFA_PathReply::xfer(flat);

  ::xfer(flat, m_entries);
}


// --------------------- RFA_MakeDirectoryRequest ----------------------
RFA_MakeDirectoryRequest::RFA_MakeDirectoryRequest()
  : RFA_PathRequest()
{}


RFA_MakeDirectoryRequest::~RFA_MakeDirectoryRequest()
{}


// ---------------------- RFA_MakeDirectoryReply -----------------------
RFA_MakeDirectoryReply::RFA_MakeDirectoryReply()
  : RFA_PathReply()
{}


RFA_MakeDirectoryReply::~RFA_MakeDirectoryReply()
{}


// EOF
