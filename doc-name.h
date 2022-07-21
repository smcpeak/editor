// doc-name.h
// DocumentName class.

#ifndef EDITOR_DOC_NAME_H
#define EDITOR_DOC_NAME_H

// smbase
#include "sm-compare.h"                // StrongOrdering, DEFINE_RELOPS_FROM_COMPARE_TO
#include "str.h"                       // string

// libc++
#include <iosfwd>                      // std::ostream


// Encapsulate the "name" of a document within the editor.  It must be
// unique (per 'compareTo') within the list of NamedTextDocuments in its
// containing NamedTextDocumentList.
class DocumentName {
private:     // data
  // Name of the resource that supplies the data.  This is a filename if
  // 'm_hasFilename' is true.  Otherwise, it is a human-readable string
  // describing the origin of the content.
  string m_resourceName;

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

  static DocumentName fromFilename(string const &filename)
    { DocumentName ret; ret.setFilename(filename); return ret; }

  // Compare by 'm_resourceName'.
  StrongOrdering compareTo(DocumentName const &obj) const;

  // A name may be empty, but not when associated with a document that
  // is part of a NamedTextDocumentList.
  bool empty() const                   { return m_resourceName.empty(); }

  // Get the resource that provides the document content.
  string resourceName() const          { return m_resourceName; }

  // True if the document's name is a file name.
  bool hasFilename() const             { return m_hasFilename; }

  // Get the filename for this document.  Requires 'hasFilename()'.
  string filename() const;

  // Set 'm_resourceName' to be 'filename', and 'm_hasFilename' to true.
  // It is the caller's responsibility to ensure uniqueness within the
  // containing NamedTextDocumentList.  This also sets 'm_directory' to
  // the directory of the file.
  void setFilename(string const &filename);

  // Set 'm_resourceName' to 'name' and 'm_hasFilename' to false.  The name
  // still has to be unique.  Sets 'm_directory' to 'dir'.
  void setNonFileResourceName(string const &name, string const &dir);

  // Get the directory associated with the document.
  string directory() const             { return m_directory; }

  // Return a string suitable for naming this document within an error
  // message.  Currently it returns resourceName() in double quotes.
  string toString() const;

  // Print 'toString()' to 'os'.
  std::ostream& print(std::ostream &os) const;
};


DEFINE_RELOPS_FROM_COMPARE_TO(DocumentName)


inline stringBuilder& operator<< (stringBuilder &sb, DocumentName const &doc)
{
  return sb << doc.toString();
}


inline std::ostream& operator<< (std::ostream &os, DocumentName const &doc)
{
  return doc.print(os);
}


#endif // EDITOR_DOC_NAME_H
