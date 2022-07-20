// doc-name.h
// DocumentName class.

#ifndef EDITOR_DOC_NAME_H
#define EDITOR_DOC_NAME_H

// smbase
#include "sm-compare.h"                // StrongOrdering
#include "str.h"                       // string


// Encapsulate the "name" of a document within the editor.
class DocumentName {
private:     // data
  // Name of document.  This is a filename if 'm_hasFilename' is true.
  // Otherwise, it is a human-readable string describing the origin of
  // the content.  It must be unique within the list of
  // NamedTextDocuments in its containing NamedTextDocumentList.  It
  // must not be empty when it is a part of such a list.
  string m_docName;

  // When true, 'm_docName' is the name of a file on disk.
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

  // Compare by 'm_docName'.
  StrongOrdering compareTo(DocumentName const &obj) const;

  // Get the document's unique (within its NamedTextDocumentList) name.
  string docName() const               { return m_docName; }

  // True if the document's name is a file name.
  bool hasFilename() const             { return m_hasFilename; }

  // Get the filename for this document.  Requires 'hasFilename()'.
  string filename() const;

  // Set 'm_docName' to be 'filename', and 'm_hasFilename' to true.  It
  // is the caller's responsibility to ensure uniqueness within the
  // containing NamedTextDocumentList.  This also sets 'm_directory' to
  // the directory of the file.
  void setFilename(string const &filename);

  // Set 'm_docName' to 'name' and 'm_hasFilename' to false.  The name
  // still has to be unique.  Sets 'm_directory' to 'dir'.
  void setNonFileName(string const &name, string const &dir);

  // Get the directory associated with the document.
  string directory() const             { return m_directory; }
};


#endif // EDITOR_DOC_NAME_H
