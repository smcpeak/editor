// named-td.h
// NamedTextDocument class.

#ifndef NAMED_TD_H
#define NAMED_TD_H

#include "named-td-fwd.h"              // fwds for this module

#include "doc-name.h"                  // DocumentName
#include "doc-type-detect.h"           // DocumentType
#include "hilite.h"                    // Highlighter
#include "td-diagnostics-fwd.h"        // TextDocumentDiagnostics
#include "td-obs-recorder.h"           // TextDocumentObservationRecorder
#include "td.h"                        // TextDocument

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/str.h"                // string

#include <cstdint>                     // std::int64_t
#include <map>                         // std::map
#include <memory>                      // std::unique_ptr
#include <optional>                    // std::optional


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

  // If set, the diagnostics associated with this document.
  std::unique_ptr<TextDocumentDiagnostics> m_diagnostics;

  // If set, an object watching `this` and updating `m_diagnostics`
  // accordingly.
  //
  // Invariant: (m_diagnostics==nullptr) == (m_tddUpdater==nullptr)
  //
  // Invariant: if m_tddUpdater!=nullptr:
  //   m_tddUpdater->getDiagnostics() == m_diagnostics &&
  //   m_tddUpdater->getDocument() == this &&
  //   m_tddUpdater->selfCheck() succeeds
  //
  // That is, the stated invariants of `m_tddUpdater` hold with respect
  // to `this` and `m_diagnostics`.
  //
  std::unique_ptr<TextDocumentDiagnosticsUpdater> m_tddUpdater;

  // Each entry in this map represents a document version that has been
  // sent to a diagnostic source (such as an LSP server).
  //
  // Invariant: If `m_diagnostics!=nullptr` and the recorder has an
  // earliest version, then:
  //
  //   m_observationRecorder.getEarliestVersion().value() >=
  //     m_diagnostics->m_originVersion
  //
  // That is, we do not keep information about document versions older
  // the one the current diagnostics were derived from.
  //
  TextDocumentObservationRecorder m_observationRecorder;

  // Which language to treat the contents as.
  DocumentType m_language;

  // Current highlighter, if any.
  std::unique_ptr<Highlighter> m_highlighter;

public:      // data
  // Modification timestamp (unix time) the last time we interacted
  // with it on the file system.
  //
  // This is 0 for an untitled document or a file that does not yet exist,
  // although there is never a reason to explicitly check for that
  // since we have 'hasFilename' for the former, and for the latter, we
  // always try to stat() the file before comparing its timestamp.
  std::int64_t m_lastFileTimestamp;

  // If true, the on-disk contents have changed since the last time we
  // saved or loaded the file.
  bool m_modifiedOnDisk;

  // Title of the document.  Must be unique within the containing
  // NamedTextDocumentList.  This will usually be similar to the name,
  // but perhaps shortened so long as it remains unique.
  string m_title;

  // When true, the widget will highlight instances of whitespace at
  // the end of a line.  Initially true, but is set to false by
  // 'setProcessOutputStatus' for other than DPS_NONE.
  //
  // In a sense, this is a sort of "overlay" highlighter, as it acts
  // after the main highlighter.  I could perhaps generalize the idea
  // of highlighting compositions at some point.
  bool m_highlightTrailingWhitespace;

  // When true, and the file is open on the LSP server, every time the
  // file is modified, we send updated contents.
  bool m_lspUpdateContinuously;

public:      // funcs
  // Create an anonymous document.  The caller must call
  // 'setDocumentName' before adding it to a document list.
  NamedTextDocument();

  ~NamedTextDocument();

  virtual void selfCheck() const override;

  virtual operator gdv::GDValue() const override;

  // Perform additional actions when setting process status.
  virtual void setDocumentProcessStatus(DocumentProcessStatus status) OVERRIDE;

  // ------------------------------ names ------------------------------
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

  // ---------------------------- language -----------------------------
  DocumentType language() const
    { return m_language; }
  Highlighter * NULLABLE highlighter() const
    { return m_highlighter.get(); }

  // Change the current language and highlighter.
  void setLanguage(DocumentType sl);

  // ---------------------------- status -------------------------------
  // Document name, process status, and unsaved changes.
  string nameWithStatusIndicators() const;

  // Empty string, plus " *" if the file has been modified in memory,
  // plus " [DISKMOD]" if the contents on disk have been modified.
  string fileStatusString() const;

  // ------------------------- file contents ---------------------------
  // Discard existing contents and set them based on the given info.
  void replaceFileAndStats(std::vector<unsigned char> const &contents,
                           std::int64_t fileModificationTime,
                           bool readOnly);

  // --------------------------- diagnostics ---------------------------
  // True if we could open this file with the LSP server.
  bool isCompatibleWithLSP() const;

  // If this file can be opened with the LSP server, return nullopt.
  // Otherwise return a user-facing explanation of why not.
  std::optional<std::string> isIncompatibleWithLSP() const;

  // Get a summary of this document's diagnostic status.
  gdv::GDValue getDiagnosticsSummary() const;

  // Get the number of diagnostics in the most recent report, or nullopt
  // if we have not received a diagnostic report.
  std::optional<int> getNumDiagnostics() const;

  // Get the version that was analyzed to produce the current set of
  // diagnostics, if there are any.
  std::optional<TD_VersionNumber> getDiagnosticsOriginVersion() const;

  // Get the current diagnostics, if any.
  TextDocumentDiagnostics const * NULLABLE getDiagnostics() const;

  // True if we have diagnostics, but they apply to a different version
  // of the document from the one we now have in memory.
  bool hasOutOfDateDiagnostics() const;

  // Set `m_diagnostics` and notify observers.  This automatically
  // adjusts the incoming diagnostics as necessary to conform to the
  // shape of the current document.
  void updateDiagnostics(
    std::unique_ptr<TextDocumentDiagnostics> diagnostics);

  // If `m_versionToObservationRecorder` contains any that are watching
  // for changes since a version before `version`, discard them.
  void removeObservationRecordersBefore(TD_VersionNumber version);

  // Get diagnostic at `tc`.  See
  // `TextDocumentDiagnostics::getDiagnosticAt` for details.
  RCSerf<TDD_Diagnostic const> getDiagnosticAt(TextMCoord tc) const;

  // We sent the current contents and version to an LSP server.  Begin
  // tracking subsequent document changes so that (1) when the
  // diagnostics arrive, we can adjust them accordingly, and (2) after
  // making some document changes, we can send incremental changes to
  // the server.
  //
  // Ensures: trackingChanges()
  void beginTrackingChanges();

  // True if we are recording changes in order to be able to send them
  // to the LSP server and to incorporate diagnostics from previous
  // versions.
  bool trackingChanges() const;

  // Return the sequence of changes that have been made to this document
  // but not yet sent to the server.
  //
  // Requires: trackingChanges()
  RCSerf<TextDocumentChangeSequence const> getUnsentChanges() const;

  // Discard all saved history related to LSP interaction.
  //
  // Ensures: !trackingChanges()
  void stopTrackingChanges();
};


#endif // NAMED_TD_H
