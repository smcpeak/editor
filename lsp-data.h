// lsp-data.h
// Language Server Protocol data structures.

// See license.txt for copyright and terms of use.

// Defines data structures specified at:
//
//   https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/
//
// Right now it's just a small subset of what is there.
//
// The classes here are prefixed with "LSP_", but otherwise named the
// same as in that document.

// I originally tried to use `astgen` to define these classes, but its
// GDV de/serialization is different from what would be needed here.  I
// could extend `astgen` to generate compatible deserialization code,
// but suspect I'll need even more flexibility.  So for now at least
// this is just hand-coded.

#ifndef EDITOR_LSP_DATA_H
#define EDITOR_LSP_DATA_H

#include "lsp-data-fwd.h"              // fwds for this module

#include "lsp-data-types.h"            // LSP_VersionNumber

#include "smbase/compare-util-iface.h" // DEFINE_FRIEND_RELATIONAL_OPERATORS
#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser

#include <iosfwd>                      // std::ostream
#include <list>                        // std::list
#include <optional>                    // std::optional
#include <string>                      // std::string


// A position in a document.
class LSP_Position final {
public:      // data
  // 0-based line number.  Non-negative.
  int m_line;

  // 0-based character number within the line.  Non-negative.
  //
  // In the absence of a different negotiated value, this counts UTF-16
  // code units.
  int m_character;

public:      // methods
  // create-tuple-class: declarations for LSP_Position +compare +write +selfCheck
  /*AUTO_CTC*/ explicit LSP_Position(int line, int character);
  /*AUTO_CTC*/ LSP_Position(LSP_Position const &obj) noexcept;
  /*AUTO_CTC*/ void selfCheck() const;
  /*AUTO_CTC*/ LSP_Position &operator=(LSP_Position const &obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(LSP_Position const &a, LSP_Position const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(LSP_Position)
  /*AUTO_CTC*/ // For +write:
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, LSP_Position const &obj);

  operator gdv::GDValue() const;

  // Parse, throwing `XGDValueError` on error.
  explicit LSP_Position(gdv::GDValueParser const &p);

  // Return the same position but at `m_character + n`.
  LSP_Position plusCharacters(int n) const;
};


// A range of characters.  The character pointed to by `m_start` is
// included in the range, while the character pointed to by `m_end` is
// *not* in the range.
class LSP_Range final {
public:      // data
  LSP_Position m_start;
  LSP_Position m_end;

public:      // methods
  // create-tuple-class: declarations for LSP_Range +compare +write
  /*AUTO_CTC*/ explicit LSP_Range(LSP_Position const &start, LSP_Position const &end);
  /*AUTO_CTC*/ LSP_Range(LSP_Range const &obj) noexcept;
  /*AUTO_CTC*/ LSP_Range &operator=(LSP_Range const &obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(LSP_Range const &a, LSP_Range const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(LSP_Range)
  /*AUTO_CTC*/ // For +write:
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, LSP_Range const &obj);

  operator gdv::GDValue() const;

  explicit LSP_Range(gdv::GDValueParser const &p);
};


// Location potentially in another file.
class LSP_Location final {
public:
  // File name, encoded as a URI.
  //
  // TODO: Wrap this in a class so I can centralize `getFname`.
  std::string m_uri;

  // Location within that file.
  LSP_Range m_range;

public:      // methods
  // create-tuple-class: declarations for LSP_Location
  /*AUTO_CTC*/ explicit LSP_Location(std::string const &uri, LSP_Range const &range);
  /*AUTO_CTC*/ LSP_Location(LSP_Location const &obj) noexcept;
  /*AUTO_CTC*/ LSP_Location &operator=(LSP_Location const &obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_Location(gdv::GDValueParser const &p);

  // Decode the URI as a file name.
  std::string getFname() const;
};


// An auxiliary message for some primary diagnostic.
class LSP_DiagnosticRelatedInformation final {
public:      // data
  // Code the message applies to, generally in a different file from the
  // main message.
  LSP_Location m_location;

  // Explanation of its relevance.
  std::string m_message;

public:      // methods
  // create-tuple-class: declarations for LSP_DiagnosticRelatedInformation
  /*AUTO_CTC*/ explicit LSP_DiagnosticRelatedInformation(LSP_Location const &location, std::string const &message);
  /*AUTO_CTC*/ LSP_DiagnosticRelatedInformation(LSP_DiagnosticRelatedInformation const &obj) noexcept;
  /*AUTO_CTC*/ LSP_DiagnosticRelatedInformation &operator=(LSP_DiagnosticRelatedInformation const &obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_DiagnosticRelatedInformation(gdv::GDValueParser const &p);
};


// One diagnostic, such as a compiler error.
class LSP_Diagnostic final {
public:      // data
  // Primary affected text range.
  LSP_Range m_range;

  // Diagnostic severity in [1,4].
  int m_severity;

  // TODO: code
  // TODO: codeDescription

  // Name of the tool or component that generated the diagnostic.
  std::optional<std::string> m_source;

  // The primary message.
  std::string m_message;

  // TODO: tags

  // Other relevant locations.  For example, when the error is a failure
  // to find a suitable overload, this will often contain the
  // candidates.
  std::list<LSP_DiagnosticRelatedInformation> m_relatedInformation;

  // TODO: data

public:      // methods
  // create-tuple-class: declarations for LSP_Diagnostic
  /*AUTO_CTC*/ explicit LSP_Diagnostic(LSP_Range const &range, int severity, std::optional<std::string> const &source, std::string const &message, std::list<LSP_DiagnosticRelatedInformation> const &relatedInformation);
  /*AUTO_CTC*/ LSP_Diagnostic(LSP_Diagnostic const &obj) noexcept;
  /*AUTO_CTC*/ LSP_Diagnostic &operator=(LSP_Diagnostic const &obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_Diagnostic(gdv::GDValueParser const &p);
};


// The data for "textDocument/publishDiagnostics".
class LSP_PublishDiagnosticsParams final {
public:      // data
  // URI of the file the diagnostics pertain to.
  std::string m_uri;

  // Document version number the diagnostics apply to.
  std::optional<LSP_VersionNumber> m_version;

  // The individual diagnostic messages.
  std::list<LSP_Diagnostic> m_diagnostics;

public:      // methods
  // create-tuple-class: declarations for LSP_PublishDiagnosticsParams
  /*AUTO_CTC*/ explicit LSP_PublishDiagnosticsParams(std::string const &uri, std::optional<LSP_VersionNumber> const &version, std::list<LSP_Diagnostic> const &diagnostics);
  /*AUTO_CTC*/ LSP_PublishDiagnosticsParams(LSP_PublishDiagnosticsParams const &obj) noexcept;
  /*AUTO_CTC*/ LSP_PublishDiagnosticsParams &operator=(LSP_PublishDiagnosticsParams const &obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_PublishDiagnosticsParams(gdv::GDValueParser const &p);
};


// The data received for "textDocument/declaration".
class LSP_LocationSequence final {
public:      // data
  // The locations of, e.g., declarations, definition, or uses,
  // depending on the originating request.
  std::list<LSP_Location> m_locations;

public:      // methods
  // create-tuple-class: declarations for LSP_LocationSequence +move
  /*AUTO_CTC*/ explicit LSP_LocationSequence(std::list<LSP_Location> const &locations);
  /*AUTO_CTC*/ explicit LSP_LocationSequence(std::list<LSP_Location> &&locations);
  /*AUTO_CTC*/ LSP_LocationSequence(LSP_LocationSequence const &obj) noexcept;
  /*AUTO_CTC*/ LSP_LocationSequence(LSP_LocationSequence &&obj) noexcept;
  /*AUTO_CTC*/ LSP_LocationSequence &operator=(LSP_LocationSequence const &obj) noexcept;
  /*AUTO_CTC*/ LSP_LocationSequence &operator=(LSP_LocationSequence &&obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_LocationSequence(gdv::GDValueParser const &p);
};


// An edit that serves as the action for a chosen completion.
class LSP_TextEdit final {
public:      // data
  // The range of text to be replaced with `newText`.  For a pure
  // insertion, the range has zero length.
  LSP_Range m_range;

  // Text to insert.
  std::string m_newText;

public:      // methods
  // create-tuple-class: declarations for LSP_TextEdit +move
  /*AUTO_CTC*/ explicit LSP_TextEdit(LSP_Range const &range, std::string const &newText);
  /*AUTO_CTC*/ explicit LSP_TextEdit(LSP_Range &&range, std::string &&newText);
  /*AUTO_CTC*/ LSP_TextEdit(LSP_TextEdit const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextEdit(LSP_TextEdit &&obj) noexcept;
  /*AUTO_CTC*/ LSP_TextEdit &operator=(LSP_TextEdit const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextEdit &operator=(LSP_TextEdit &&obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_TextEdit(gdv::GDValueParser const &p);
};


// One possible completion for "textDocument/completion".
class LSP_CompletionItem final {
public:      // data
  // The completion "label".  I take it this is the text that should
  // appear in the list of choices presented to the user.  It is also
  // "by default" (?) the text that gets inserted into the code if the
  // user chooses this item.
  std::string m_label;

  // TODO: labelDetails

  // TODO: kind

  // TODO: tags

  // TODO: detail

  // TODO: documentation

  // TODO: deprecated

  // TODO: preselect

  // TODO: Sorting key.
  //std::optional<std::string> m_sortText;

  // TODO: filterText

  // Ignored: `insertText`
  //
  // We use `textEdit` instead.

  // TODO: insertTextFormat

  // TODO: insertTextMode

  // The edit to perform if this item is chosen.
  //
  // The spec treats this as optional, but I assume it is always present
  // since it is the only method my editor can handle.
  //
  // TODO: The spec allows `InsertReplaceEdit` here too.
  LSP_TextEdit m_textEdit;

  // TODO: textEditText

  // TODO: additionalTextEdits

  // TODO: commitCharacters

  // TODO: command

  // TODO: data

  // TODO: `clangd` also provides a "score".

public:      // methods
  // create-tuple-class: declarations for LSP_CompletionItem +move
  /*AUTO_CTC*/ explicit LSP_CompletionItem(std::string const &label, LSP_TextEdit const &textEdit);
  /*AUTO_CTC*/ explicit LSP_CompletionItem(std::string &&label, LSP_TextEdit &&textEdit);
  /*AUTO_CTC*/ LSP_CompletionItem(LSP_CompletionItem const &obj) noexcept;
  /*AUTO_CTC*/ LSP_CompletionItem(LSP_CompletionItem &&obj) noexcept;
  /*AUTO_CTC*/ LSP_CompletionItem &operator=(LSP_CompletionItem const &obj) noexcept;
  /*AUTO_CTC*/ LSP_CompletionItem &operator=(LSP_CompletionItem &&obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_CompletionItem(gdv::GDValueParser const &p);
};


// The data received for "textDocument/completion".
class LSP_CompletionList final {
public:      // data
  // The list is not complete.
  //
  // I think this implies another message might arrive with a
  // complete list?  The spec doesn't really say.
  bool m_isIncomplete;

  // TODO: itemDefaults

  // The list of completions.
  std::list<LSP_CompletionItem> m_items;

public:      // methods
  // create-tuple-class: declarations for LSP_CompletionList +move
  /*AUTO_CTC*/ explicit LSP_CompletionList(bool isIncomplete, std::list<LSP_CompletionItem> const &items);
  /*AUTO_CTC*/ explicit LSP_CompletionList(bool isIncomplete, std::list<LSP_CompletionItem> &&items);
  /*AUTO_CTC*/ LSP_CompletionList(LSP_CompletionList const &obj) noexcept;
  /*AUTO_CTC*/ LSP_CompletionList(LSP_CompletionList &&obj) noexcept;
  /*AUTO_CTC*/ LSP_CompletionList &operator=(LSP_CompletionList const &obj) noexcept;
  /*AUTO_CTC*/ LSP_CompletionList &operator=(LSP_CompletionList &&obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_CompletionList(gdv::GDValueParser const &p);
};


// Document identifier without specified version.
class LSP_TextDocumentIdentifier {
public:      // data
  // File name, essentially.
  std::string m_uri;

public:      // methods
  // create-tuple-class: declarations for LSP_TextDocumentIdentifier +move
  /*AUTO_CTC*/ explicit LSP_TextDocumentIdentifier(std::string const &uri);
  /*AUTO_CTC*/ explicit LSP_TextDocumentIdentifier(std::string &&uri);
  /*AUTO_CTC*/ LSP_TextDocumentIdentifier(LSP_TextDocumentIdentifier const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentIdentifier(LSP_TextDocumentIdentifier &&obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentIdentifier &operator=(LSP_TextDocumentIdentifier const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentIdentifier &operator=(LSP_TextDocumentIdentifier &&obj) noexcept;

  operator gdv::GDValue() const;

  // No need to parse currently.

  // Encode `fname` as a URI to build this object.
  static LSP_TextDocumentIdentifier fromFname(
    std::string const &fname);

  // Decode the URI as a file name.
  std::string getFname() const;
};


// Identifier of a specific document version.  This is used, among other
// things, when sending the "didChange" notification.
//
// The real protocol describes this as inheriting
// `LSP_TextDocumentIdentifier`, but my create-tuple-class script does
// not handle that right, so I embed its one `m_uri` field.
//
// TODO: Fix the script so I can use inheritance here.
//
class LSP_VersionedTextDocumentIdentifier {
public:      // data
  // File name, essentially.
  std::string m_uri;

  // The version.
  LSP_VersionNumber m_version;

public:      // methods
  // create-tuple-class: declarations for LSP_VersionedTextDocumentIdentifier +move
  /*AUTO_CTC*/ explicit LSP_VersionedTextDocumentIdentifier(std::string const &uri, LSP_VersionNumber const &version);
  /*AUTO_CTC*/ explicit LSP_VersionedTextDocumentIdentifier(std::string &&uri, LSP_VersionNumber &&version);
  /*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier(LSP_VersionedTextDocumentIdentifier const &obj) noexcept;
  /*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier(LSP_VersionedTextDocumentIdentifier &&obj) noexcept;
  /*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier &operator=(LSP_VersionedTextDocumentIdentifier const &obj) noexcept;
  /*AUTO_CTC*/ LSP_VersionedTextDocumentIdentifier &operator=(LSP_VersionedTextDocumentIdentifier &&obj) noexcept;

  operator gdv::GDValue() const;

  // No need to parse currently.

  // Encode `fname` as a URI to build this object.
  static LSP_VersionedTextDocumentIdentifier fromFname(
    std::string const &fname,
    LSP_VersionNumber version);

  // Decode the URI as a file name.
  std::string getFname() const;
};


// One change to a document.
class LSP_TextDocumentContentChangeEvent {
public:      // data
  // The range to replace with `m_text`.  If this is absent, the text
  // replaces the entire document.
  std::optional<LSP_Range> m_range;

  // The real LSP protocol has an optional but deprecated `rangeLength`
  // here, which I ignore.

  // New text for the range or document.
  std::string m_text;

public:      // methods
  // create-tuple-class: declarations for LSP_TextDocumentContentChangeEvent +move
  /*AUTO_CTC*/ explicit LSP_TextDocumentContentChangeEvent(std::optional<LSP_Range> const &range, std::string const &text);
  /*AUTO_CTC*/ explicit LSP_TextDocumentContentChangeEvent(std::optional<LSP_Range> &&range, std::string &&text);
  /*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent(LSP_TextDocumentContentChangeEvent const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent(LSP_TextDocumentContentChangeEvent &&obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent &operator=(LSP_TextDocumentContentChangeEvent const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentContentChangeEvent &operator=(LSP_TextDocumentContentChangeEvent &&obj) noexcept;

  operator gdv::GDValue() const;

  // No need to parse currently.
};


// Parameters for "textDocument/didChange".
class LSP_DidChangeTextDocumentParams {
public:      // data
  // The document that changed.  The version is *after* the changes.
  LSP_VersionedTextDocumentIdentifier m_textDocument;

  // The changes to apply, in order.
  std::list<LSP_TextDocumentContentChangeEvent> m_contentChanges;

public:      // methods
  // create-tuple-class: declarations for LSP_DidChangeTextDocumentParams +move
  /*AUTO_CTC*/ explicit LSP_DidChangeTextDocumentParams(LSP_VersionedTextDocumentIdentifier const &textDocument, std::list<LSP_TextDocumentContentChangeEvent> const &contentChanges);
  /*AUTO_CTC*/ explicit LSP_DidChangeTextDocumentParams(LSP_VersionedTextDocumentIdentifier &&textDocument, std::list<LSP_TextDocumentContentChangeEvent> &&contentChanges);
  /*AUTO_CTC*/ LSP_DidChangeTextDocumentParams(LSP_DidChangeTextDocumentParams const &obj) noexcept;
  /*AUTO_CTC*/ LSP_DidChangeTextDocumentParams(LSP_DidChangeTextDocumentParams &&obj) noexcept;
  /*AUTO_CTC*/ LSP_DidChangeTextDocumentParams &operator=(LSP_DidChangeTextDocumentParams const &obj) noexcept;
  /*AUTO_CTC*/ LSP_DidChangeTextDocumentParams &operator=(LSP_DidChangeTextDocumentParams &&obj) noexcept;

  operator gdv::GDValue() const;

  // No need to parse currently.

  // Get the file name in `m_textDocument`.
  std::string getFname() const;
};


// Parameters for symbol queries:  "textDocument/{declaration,
// definition, hover, completion}".
class LSP_TextDocumentPositionParams {
public:      // data
  // The document for which we want information.
  LSP_TextDocumentIdentifier m_textDocument;

  // The location of an occurrence of the symbol of interest.
  LSP_Position m_position;

public:      // methods
  // create-tuple-class: declarations for LSP_TextDocumentPositionParams +move
  /*AUTO_CTC*/ explicit LSP_TextDocumentPositionParams(LSP_TextDocumentIdentifier const &textDocument, LSP_Position const &position);
  /*AUTO_CTC*/ explicit LSP_TextDocumentPositionParams(LSP_TextDocumentIdentifier &&textDocument, LSP_Position &&position);
  /*AUTO_CTC*/ LSP_TextDocumentPositionParams(LSP_TextDocumentPositionParams const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentPositionParams(LSP_TextDocumentPositionParams &&obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentPositionParams &operator=(LSP_TextDocumentPositionParams const &obj) noexcept;
  /*AUTO_CTC*/ LSP_TextDocumentPositionParams &operator=(LSP_TextDocumentPositionParams &&obj) noexcept;

  operator gdv::GDValue() const;

  // No need to parse currently.

  // Get the file name in `m_textDocument`.
  std::string getFname() const;
};


#endif // EDITOR_LSP_DATA_H
