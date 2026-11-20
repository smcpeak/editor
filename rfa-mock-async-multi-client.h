// rfa-mock-async-multi-client.h
// `RFA_MockAsyncMultiClient`, an RFA mock object for testing.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_MOCK_ASYNC_MULTI_CLIENT_H
#define EDITOR_RFA_MOCK_ASYNC_MULTI_CLIENT_H

#include "host-name-fwd.h"                       // HostName [n]
#include "rfa-async-multi-client.h"              // RFA_AbstractAsyncMultiClient
#include "rfa-msg-fwd.h"                         // RFA_ReadFile{Request,Reply} [n]

#include "smbase/either-fwd.h"                   // smbase::Either
#include "smbase/portable-error-code-fwd.h"      // smbase::PortableErrorCode [n]

#include <QObject>                               // Q_OBJECT, Q_SIGNALS, Q_SLOTS

#include <map>                                   // std::map
#include <memory>                                // std::unique_ptr
#include <string>                                // std::string


// RFA access for testing purposes, without actual IPC.
class RFA_MockAsyncMultiClient : public RFA_AbstractAsyncMultiClient {
  Q_OBJECT
  NO_OBJECT_COPIES(RFA_MockAsyncMultiClient);

public:      // types
  // Data we know about one file: either its contents for a successful
  // read, or the error code.
  using FileReplyData =
    smbase::Either<std::string, smbase::PortableErrorCode>;

public:      // data
  // Hosts considered connected.
  std::map<HostName, ConnectionState> m_hosts;

  // Issued requests.
  std::map<RequestID, std::unique_ptr<RFA_Message>> m_issuedRequests;

  // Map from file names to reply data.
  std::map<std::string, FileReplyData> m_files;

protected Q_SLOTS:
  // Process the enqueued requests.
  void slot_processRequests();

  // Process the RFR request.
  std::unique_ptr<RFA_Msg_ReadFileReply> processRFR(
    RFA_Msg_ReadFileRequest const *rfr);

public:      // methods
  virtual ~RFA_MockAsyncMultiClient() override;

  // Initially empty maps.
  RFA_MockAsyncMultiClient();

  virtual void selfCheck() const override;

  // ------------------ RFA_AsyncMultiClient methods -------------------
  virtual ConnectionState connectionState(
    HostName const &hostName) const override;
  virtual void issueRequest(
    RequestID &requestID /*OUT*/,
    HostName const &hostName,
    std::unique_ptr<RFA_Message> req) override;
  virtual bool requestIsOutstanding(RequestID requestID) const override;
  virtual void cancelRequest(RequestID requestID) override;
  virtual int numOutstandingRequests() const override;

Q_SIGNALS:
  // Emitted when a request is enqueued.
  void signal_processRequests();
};


#endif // EDITOR_RFA_MOCK_ASYNC_MULTI_CLIENT_H
