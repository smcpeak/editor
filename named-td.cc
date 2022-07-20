// named-td.cc
// code for named-td.h

#include "named-td.h"                  // this module

// smbase
#include "dev-warning.h"               // DEV_WARNING
#include "sm-macros.h"                 // CMEMB
#include "objcount.h"                  // CHECK_OBJECT_COUNT
#include "sm-file-util.h"              // SMFileUtil
#include "trace.h"                     // TRACE


int NamedTextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(NamedTextDocument);


NamedTextDocument::NamedTextDocument()
  : TextDocument(),
    m_docName(),
    m_hasFilename(false),
    m_directory(),
    m_lastFileTimestamp(0),
    m_modifiedOnDisk(false),
    m_title(),
    m_highlighter(NULL),
    m_highlightTrailingWhitespace(true)
{
  NamedTextDocument::s_objectCount++;
}

NamedTextDocument::~NamedTextDocument()
{
  NamedTextDocument::s_objectCount--;
  if (m_highlighter) {
    delete m_highlighter;
  }
}


void NamedTextDocument::setDocumentProcessStatus(DocumentProcessStatus status)
{
  this->TextDocument::setDocumentProcessStatus(status);

  if (this->isProcessOutput()) {
    this->m_highlightTrailingWhitespace = false;
  }
}


string NamedTextDocument::filename() const
{
  xassert(this->hasFilename());
  return m_docName;
}


void NamedTextDocument::setDirectory(string const &dir)
{
  SMFileUtil sfu;
  m_directory = sfu.ensureEndsWithDirectorySeparator(
    sfu.normalizePathSeparators(dir));
}


void NamedTextDocument::setFilename(string const &filename)
{
  m_docName = filename;
  m_hasFilename = true;

  string dir, base;
  SMFileUtil().splitPath(dir, base, filename);
  this->setDirectory(dir);
}


void NamedTextDocument::setNonFileName(string const &name, string const &dir)
{
  m_docName = name;
  m_hasFilename = false;

  this->setDirectory(dir);
}


static char const *documentProcessStatusIndicator(
  NamedTextDocument const *doc)
{
  switch (doc->documentProcessStatus()) {
    default:
      DEV_WARNING("Bad document process status");
      // fallthrough
    case DPS_NONE:           return "";
    case DPS_RUNNING:        return "<running> ";
    case DPS_FINISHED:       return "<finished> ";
  }
}

string NamedTextDocument::nameWithStatusIndicators() const
{
  stringBuilder sb;
  sb << documentProcessStatusIndicator(this);
  sb << this->docName();
  sb << fileStatusString();
  return sb;
}


string NamedTextDocument::fileStatusString() const
{
  stringBuilder sb;
  if (this->unsavedChanges()) {
    sb << " *";
  }
  if (m_modifiedOnDisk) {
    sb << " [DISKMOD]";
  }
  return sb.str();
}


void NamedTextDocument::replaceFileAndStats(
  std::vector<unsigned char> const &contents,
  int64_t fileModificationTime,
  bool readOnly)
{
  TRACE("NamedTextDocument",
    "replaceFileAndStats: docName=" << m_docName <<
    " contents.size()=" << contents.size() <<
    " modTime=" << fileModificationTime <<
    " readOnly=" << readOnly);

  this->replaceWholeFile(contents);
  this->m_lastFileTimestamp = fileModificationTime;
  this->m_modifiedOnDisk = false;
  this->setReadOnly(readOnly);
}


// EOF
