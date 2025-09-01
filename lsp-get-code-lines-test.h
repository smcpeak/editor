// lsp-get-code-lines-test.h
// Decls for `lsp-get-code-lines-test.cc`.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_LSP_GET_CODE_LINES_TEST_H
#define EDITOR_LSP_GET_CODE_LINES_TEST_H

#include "host-name-fwd.h"             // HostName [n]
#include "vfs-connections.h"           // VFS_Connections
#include "vfs-msg-fwd.h"               // VFS_ReadFile{Request,Reply} [n]

#include <map>                         // std::map
#include <memory>                      // std::unique_ptr
#include <string>                      // std::string


// VFS access for testing purposes, without actual IPC.
class VFS_TestConnections : public VFS_AbstractConnections {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_TestConnections);

public:      // data
  // Hosts considered connected.
  std::map<HostName, ConnectionState> m_hosts;

  // Issued requests.
  std::map<RequestID, std::unique_ptr<VFS_Message>> m_issuedRequests;

  // Map from file names to contents to be served as replies.
  std::map<std::string, std::string> m_files;

protected Q_SLOTS:
  // Process the enqueued requests.
  void slot_processRequests();

  // Process the RFR request.
  std::unique_ptr<VFS_ReadFileReply> processRFR(
    VFS_ReadFileRequest const *rfr);

public:      // methods
  VFS_TestConnections();

  virtual void selfCheck() const override;

  // --------------------- VFS_Connections methods ---------------------
  virtual ConnectionState connectionState(
    HostName const &hostName) const override;
  virtual void issueRequest(
    RequestID &requestID /*OUT*/,
    HostName const &hostName,
    std::unique_ptr<VFS_Message> req) override;
  virtual bool requestIsOutstanding(RequestID requestID) const override;
  virtual void cancelRequest(RequestID requestID) override;
  virtual int numOutstandingRequests() const override;

Q_SIGNALS:
  // Emitted when a request is enqueued.
  void signal_processRequests();
};


#endif // EDITOR_LSP_GET_CODE_LINES_TEST_H
