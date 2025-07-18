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

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue
#include "smbase/gdvalue-parser-fwd.h" // gdv::GDValueParser

#include <list>                        // std::list
#include <string>                      // std::string


// A position in a document.
class LSP_Position final {
public:      // data
  // 0-based line number.
  int m_line;

  // 0-based character number within the line.
  //
  // In the absence of a different negotiated value, this counts UTF-16
  // code units.
  int m_character;

public:      // methods
  // create-tuple-class: declarations for LSP_Position
  /*AUTO_CTC*/ explicit LSP_Position(int line, int character);
  /*AUTO_CTC*/ LSP_Position(LSP_Position const &obj) noexcept;
  /*AUTO_CTC*/ LSP_Position &operator=(LSP_Position const &obj) noexcept;

  operator gdv::GDValue() const;

  // Parse, throwing `XGDValueError` on error.
  explicit LSP_Position(gdv::GDValueParser const &p);
};


// A range of characters.  The character pointed to by `m_start` is
// included in the range, while the character pointed to by `m_end` is
// *not* in the range.
class LSP_Range final {
public:      // data
  LSP_Position m_start;
  LSP_Position m_end;

public:      // methods
  // create-tuple-class: declarations for LSP_Range
  /*AUTO_CTC*/ explicit LSP_Range(LSP_Position const &start, LSP_Position const &end);
  /*AUTO_CTC*/ LSP_Range(LSP_Range const &obj) noexcept;
  /*AUTO_CTC*/ LSP_Range &operator=(LSP_Range const &obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_Range(gdv::GDValueParser const &p);
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
  std::string m_source;

  // The primary message.
  std::string m_message;

  // TODO: tags
  // TODO: relatedInformation
  // TODO: data

public:      // methods
  // create-tuple-class: declarations for LSP_Diagnostic
  /*AUTO_CTC*/ explicit LSP_Diagnostic(LSP_Range const &range, int severity, std::string const &source, std::string const &message);
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
  int m_version;

  // The individual diagnostic messages.
  std::list<LSP_Diagnostic> m_diagnostics;

public:      // methods
  // create-tuple-class: declarations for LSP_PublishDiagnosticsParams
  /*AUTO_CTC*/ explicit LSP_PublishDiagnosticsParams(std::string const &uri, int version, std::list<LSP_Diagnostic> const &diagnostics);
  /*AUTO_CTC*/ LSP_PublishDiagnosticsParams(LSP_PublishDiagnosticsParams const &obj) noexcept;
  /*AUTO_CTC*/ LSP_PublishDiagnosticsParams &operator=(LSP_PublishDiagnosticsParams const &obj) noexcept;

  operator gdv::GDValue() const;

  explicit LSP_PublishDiagnosticsParams(gdv::GDValueParser const &p);
};


#endif // EDITOR_LSP_DATA_H
