// named-td.cc
// code for named-td.h

#include "named-td.h"                  // this module

#include "lsp-data.h"                  // LSP_PublishDiagnosticsParams
#include "td-diagnostics.h"            // TextDocumentDiagnostics

#include "smbase/dev-warning.h"        // DEV_WARNING
#include "smbase/objcount.h"           // CHECK_OBJECT_COUNT
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // CMEMB
#include "smbase/string-util.h"        // replaceAll
#include "smbase/trace.h"              // TRACE

#include <cstdint>                     // std::int64_t


int NamedTextDocument::s_objectCount = 0;

CHECK_OBJECT_COUNT(NamedTextDocument);


NamedTextDocument::NamedTextDocument()
  : TextDocument(),
    m_documentName(),
    m_lastFileTimestamp(0),
    m_modifiedOnDisk(false),
    m_title(),
    m_highlighter(),
    m_highlightTrailingWhitespace(true),
    m_lspDiagnostics(),
    m_diagnostics()
{
  NamedTextDocument::s_objectCount++;
}

NamedTextDocument::~NamedTextDocument()
{
  NamedTextDocument::s_objectCount--;
}


void NamedTextDocument::setDocumentProcessStatus(DocumentProcessStatus status)
{
  this->TextDocument::setDocumentProcessStatus(status);

  if (this->isProcessOutput()) {
    this->m_highlightTrailingWhitespace = false;
  }
}


HostAndResourceName NamedTextDocument::directoryHarn() const
{
  return HostAndResourceName(hostName(), directory());
}


string NamedTextDocument::applyCommandSubstitutions(string const &orig) const
{
  // If the document does not have a file name, we will return a pair of
  // quotes, signifying an empty string in the context of a shell
  // command.
  string fname = "''";

  if (m_documentName.hasFilename()) {
    SMFileUtil sfu;
    fname = sfu.splitPathBase(m_documentName.filename());
  }

  // Very simple, naive implementation.
  return replaceAll(orig, "$f", fname);
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
  if (!hostName().isLocal()) {
    sb << hostName() << ": ";
  }
  sb << resourceName();
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
  std::int64_t fileModificationTime,
  bool readOnly)
{
  TRACE("NamedTextDocument",
    "replaceFileAndStats: docName=" << documentName() <<
    " contents.size()=" << contents.size() <<
    " modTime=" << fileModificationTime <<
    " readOnly=" << readOnly);

  this->replaceWholeFile(contents);
  this->m_lastFileTimestamp = fileModificationTime;
  this->m_modifiedOnDisk = false;
  this->setReadOnly(readOnly);
}


// EOF
