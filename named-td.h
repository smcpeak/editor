// named-td.h
// NamedTextDocument class.

#ifndef NAMED_TD_H
#define NAMED_TD_H

// editor
#include "doc-name.h"                  // DocumentName
#include "hilite.h"                    // Highlighter
#include "td.h"                        // TextDocument

// smbase
#include "str.h"                       // string

// libc
#include <stdint.h>                    // int64_t


// This class binds a TextDocument, which is an abstract mathematical
// object, to a name, which refers somehow to the origin of the
// contents of the document.  Often, this is a file name, but it can
// also be, for example, a command line.
//
// If the name refers to a durable external resource, such as a file,
// then the document is saved to, loaded from, and checked against the
// resource at appropriate points.
//
// This class further associates that binding with the way of naming it
// from within the editor application, which is the document title.
//
// Finally, it contains an interpretation of the document's meaning in
// the form of a syntax highlighter.
//
// All of the data in this class is shared by all editor windows that
// operate on a given document.
class NamedTextDocument : public TextDocument {
public:      // static data
  static int s_objectCount;

private:     // data
  // File name, etc.  Unique within the containing
  // NamedTextDocumentList.
  DocumentName m_documentName;

public:      // data
  // Modification timestamp (unix time) the last time we interacted
  // with it on the file system.
  //
  // This is 0 for an untitled document or a file that does not yet exist,
  // although there is never a reason to explicitly check for that
  // since we have 'hasFilename' for the former, and for the latter, we
  // always try to stat() the file before comparing its timestamp.
  int64_t m_lastFileTimestamp;

  // If true, the on-disk contents have changed since the last time we
  // saved or loaded the file.
  bool m_modifiedOnDisk;

  // Title of the document.  Must be unique within the containing
  // NamedTextDocumentList.  This will usually be similar to the name,
  // but perhaps shortened so long as it remains unique.
  string m_title;

  // current highlighter; clients can come in and replace the
  // highlighter, but it must always be the case that the
  // highlighter is attached to 'this' buffer (because it's allowed
  // to maintain internal incremental state about the buffer
  // contents)
  Highlighter *m_highlighter;      // (nullable owner)

  // When true, the widget will highlight instances of whitespace at
  // the end of a line.  Initially true, but is set to false by
  // 'setProcessOutputStatus' for other than DPS_NONE.
  //
  // In a sense, this is a sort of "overlay" highlighter, as it acts
  // after the main highlighter.  I could perhaps generalize the idea
  // of highlighting compositions at some point.
  bool m_highlightTrailingWhitespace;

public:      // funcs
  // Create an anonymous document.  The caller must call
  // 'setDocumentName' before adding it to a document list.
  NamedTextDocument();

  ~NamedTextDocument();

  // Perform additional actions when setting process status.
  virtual void setDocumentProcessStatus(DocumentProcessStatus status) OVERRIDE;

  // ----------------------------- names ----------------------------
  DocumentName const &documentName() const
    { return m_documentName; }
  void setDocumentName(DocumentName const &docName)
    { m_documentName = docName; }

  HostAndResourceName const &harn() const { return m_documentName.harn(); }

  // Host name and directory.
  //
  // Like 'directory()', the directory here includes the trailing slash.
  // That is important because this flows into nearby-file, which then
  // feeds into filename-input, which wants the trailing slash in order
  // to show the contents of a directory.
  HostAndResourceName directoryHarn() const;

  // See comments on the corresponding methods of DocumentName.
  HostName hostName() const            { return m_documentName.hostName(); }
  string resourceName() const          { return m_documentName.resourceName(); }
  bool hasFilename() const             { return m_documentName.hasFilename(); }
  string filename() const              { return m_documentName.filename(); }
  string directory() const             { return m_documentName.directory(); }

  // ---------------------------- status -------------------------------
  // Document name, process status, and unsaved changes.
  string nameWithStatusIndicators() const;

  // Empty string, plus " *" if the file has been modified in memory,
  // plus " [DISKMOD]" if the contents on disk have been modified.
  string fileStatusString() const;

  // ------------------------- file contents ---------------------------
  // Discard existing contents and set them based on the given info.
  void replaceFileAndStats(std::vector<unsigned char> const &contents,
                           int64_t fileModificationTime,
                           bool readOnly);
};

#endif // NAMED_TD_H
