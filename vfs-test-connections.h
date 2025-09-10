// vfs-test-connections.h
// `VFS_TestConnections`, a VFS mock object for testing.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_VFS_TEST_CONNECTIONS_H
#define EDITOR_VFS_TEST_CONNECTIONS_H

#include "host-name-fwd.h"                       // HostName [n]
#include "vfs-connections.h"                     // VFS_AbstractConnections
#include "vfs-msg-fwd.h"                         // VFS_ReadFile{Request,Reply} [n]

#include "smbase/either-fwd.h"                   // smbase::Either
#include "smbase/portable-error-code-fwd.h"      // smbase::PortableErrorCode [n]

#include <map>                                   // std::map
#include <memory>                                // std::unique_ptr
#include <string>                                // std::string


// VFS access for testing purposes, without actual IPC.
class VFS_TestConnections : public VFS_AbstractConnections {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_TestConnections);

public:      // types
  // Data we know about one file: either its contents for a successful
  // read, or the error code.
  using FileReplyData =
    smbase::Either<std::string, smbase::PortableErrorCode>;

public:      // data
  // Hosts considered connected.
  std::map<HostName, ConnectionState> m_hosts;

  // Issued requests.
  std::map<RequestID, std::unique_ptr<VFS_Message>> m_issuedRequests;

  // Map from file names to reply data.
  std::map<std::string, FileReplyData> m_files;

protected Q_SLOTS:
  // Process the enqueued requests.
  void slot_processRequests();

  // Process the RFR request.
  std::unique_ptr<VFS_ReadFileReply> processRFR(
    VFS_ReadFileRequest const *rfr);

public:      // methods
  virtual ~VFS_TestConnections() override;

  // Initially empty maps.
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


#endif // EDITOR_VFS_TEST_CONNECTIONS_H
