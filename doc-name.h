// doc-name.h
// DocumentName class.

#ifndef EDITOR_DOC_NAME_H
#define EDITOR_DOC_NAME_H

// editor
#include "host-and-resource-name.h"    // HostAndResourceName, HostName

// smbase
#include "sm-compare.h"                // StrongOrdering, DEFINE_RELOPS_FROM_COMPARE_TO
#include "str.h"                       // string

// libc++
#include <iosfwd>                      // std::ostream


// Encapsulate the "name" of a document within the editor.  It must be
// unique (per 'compareTo') within the list of NamedTextDocuments in its
// containing NamedTextDocumentList.
//
// HostAndResourceName::m_resourceName is a filename if 'm_hasFilename'
// is true.  Otherwise, it is a human-readable string describing the
// origin of the content.
class DocumentName : public HostAndResourceName {
private:     // data
  // When true, 'm_resourceName' is the name of a file on disk.
  bool m_hasFilename;

  // Directory associated with this document.  For a file, this is the
  // directory containing the file.  For process output, it is the
  // working directory of the process.  For others, it's somewhat
  // arbitrary, with the working directory of the editor itself acting
  // as the final fallback.  It must always end with a path separator
  // character, and it only uses '/' as the separator, even on Windows.
  string m_directory;

private:     // funcs
  void setDirectory(string const &dir);

public:      // methods
  // Create an empty name.
  DocumentName();
  ~DocumentName();

  DocumentName(DocumentName const &obj) = default;
  DocumentName& operator=(DocumentName const &obj) = default;

  static DocumentName fromFilename(HostName const &hostName,
                                   string const &filename)
  {
    DocumentName ret;
    ret.setFilename(hostName, filename);
    return ret;
  }

  static DocumentName fromNonFileResourceName(HostName const &hostName,
    string const &name, string const &dir)
  {
    DocumentName ret;
    ret.setNonFileResourceName(hostName, name, dir);
    return ret;
  }

  // Assert invariants.
  void selfCheck() const;

  // Compare by host and resource name.
  StrongOrdering compareTo(DocumentName const &obj) const;

  // True if the document's name is a file name.
  bool hasFilename() const             { return m_hasFilename; }

  // Get the filename for this document.  Requires 'hasFilename()'.
  string filename() const;

  // Set 'm_hostName' to 'hostName, 'm_resourceName' to be 'filename',
  // and 'm_hasFilename' to true.  It is the caller's responsibility to
  // ensure uniqueness within the containing NamedTextDocumentList.
  // This also sets 'm_directory' to the directory of the file.
  void setFilename(HostName const &hostName, string const &filename);

  // Set 'm_hostName' to 'hostName, 'm_resourceName' to 'name', and
  // 'm_hasFilename' to false.  The name still has to be unique.  Sets
  // 'm_directory' to 'dir'.
  void setNonFileResourceName(HostName const &hostName,
    string const &name, string const &dir);

  // Get the directory associated with the document.
  string directory() const             { return m_directory; }
};


DEFINE_RELOPS_FROM_COMPARE_TO(DocumentName)


#endif // EDITOR_DOC_NAME_H
