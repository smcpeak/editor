// lsp-client-scope.h
// `LSPClientScope, the scope of an LSP server.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_CLIENT_SCOPE_H
#define EDITOR_LSP_CLIENT_SCOPE_H

#include "lsp-client-scope-fwd.h"      // fwds for this module

#include "doc-type.h"                  // DocumentType
#include "host-name.h"                 // HostName
#include "named-td-fwd.h"              // NamedTextDocument [n]

#include "smbase/compare-util-iface.h" // DEFINE_FRIEND_RELATIONAL_OPERATORS


// Describes the scope of a potential LSP client-server connection.
class LSPClientScope {
public:      // data
  // Host on which the LSP server is running.
  //
  // TODO: For now, this is always just the local host.
  HostName m_hostName;

  // Type of document that this server handles.
  //
  // Currently I assume each server only handles one document type,
  // but in the future I might need to generalize this.
  DocumentType m_documentType;

public:
  // ---- create-tuple-class: declarations for LSPClientScope +compare +gdvWrite +move
  /*AUTO_CTC*/ explicit LSPClientScope(HostName const &hostName, DocumentType const &documentType);
  /*AUTO_CTC*/ explicit LSPClientScope(HostName &&hostName, DocumentType &&documentType);
  /*AUTO_CTC*/ LSPClientScope(LSPClientScope const &obj) noexcept;
  /*AUTO_CTC*/ LSPClientScope(LSPClientScope &&obj) noexcept;
  /*AUTO_CTC*/ LSPClientScope &operator=(LSPClientScope const &obj) noexcept;
  /*AUTO_CTC*/ LSPClientScope &operator=(LSPClientScope &&obj) noexcept;
  /*AUTO_CTC*/ // For +compare:
  /*AUTO_CTC*/ friend int compare(LSPClientScope const &a, LSPClientScope const &b);
  /*AUTO_CTC*/ DEFINE_FRIEND_RELATIONAL_OPERATORS(LSPClientScope)
  /*AUTO_CTC*/ // For +gdvWrite:
  /*AUTO_CTC*/ operator gdv::GDValue() const;
  /*AUTO_CTC*/ std::string toString() const;
  /*AUTO_CTC*/ void write(std::ostream &os) const;
  /*AUTO_CTC*/ friend std::ostream &operator<<(std::ostream &os, LSPClientScope const &obj);

  // Return the scope applicable to `ntd`.
  static LSPClientScope forNTD(NamedTextDocument const *ntd);

  // Return: ::languageName(m_documentType)
  std::string languageName() const;

  // Return: m_hostName.toString()
  std::string hostString() const;

  // Return: "<languageName()> files on <hostString()> host"
  //
  // Example: "C++ files on local host"
  std::string description() const;
};


#endif // EDITOR_LSP_CLIENT_SCOPE_H
