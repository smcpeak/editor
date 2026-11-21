// rfa-sync-multi-client.h
// RFA_SyncMultiClient class.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_SYNC_MULTI_CLIENT_H
#define EDITOR_RFA_SYNC_MULTI_CLIENT_H

// editor
#include "host-and-resource-name.h"              // HostAndResourceName
#include "host-name.h"                           // HostName
#include "reply-ptr-opt.h"                       // ReplyPtrOpt, ReplyPtrOptOrError
#include "rfa-abstract-sync-multi-client.h"      // RFA_AbstractSyncMultiClient
#include "rfa-async-multi-client.h"              // RFA_AbstractAsyncMultiClient
#include "rfa-msg.h"                             // RFA_Message

// smqtutil
#include "smqtutil/sync-wait-fwd.h"              // SynchronousWaiter [f]

// smbase
#include "smbase/sm-unique-ptr.h"                // smbase::UniquePtr

// Qt
#include <QObject>


// Implementation of `RFA_AbstractSyncMultiClient` on top of
// `RFA_AbstractSyncMultiClient` that (via `SynchronousWaiter`) can be
// hooked up to a GUI to allow the user to cancel requests.
class RFA_SyncMultiClient : public QObject,
                            public RFA_AbstractSyncMultiClient {
  Q_OBJECT
  NO_OBJECT_COPIES(RFA_SyncMultiClient);

public:      // types
  typedef RFA_AbstractAsyncMultiClient::RequestID RequestID;

private:     // data
  // Query interface to use.
  RFA_AbstractAsyncMultiClient *m_rfaAMClient;

  // Wait mechanism.
  SynchronousWaiter &m_waiter;

  // ID of outstanding request, or 0 if none.
  RequestID m_requestID;

  // If 'm_requestID' is not zero, this is the host being queried.
  // Otherwise, it is meaningless.
  HostName m_hostName;

  // Reply, if one has arrived.
  ReplyPtrOpt<RFA_Message> m_reply;

  // Connection lost message, or empty string.
  string m_connLostMessage;

public:      // instance methods
  // Create an object to issue queries via `rfaAMClient`.  Use
  // `waiter` to wait, which can (e.g.) pop up a modal window.
  RFA_SyncMultiClient(
    RFA_AbstractAsyncMultiClient *rfaAMClient, SynchronousWaiter &waiter);

  virtual ~RFA_SyncMultiClient() override;

  // Issue 'request' and wait for the reply.
  virtual ReplyPtrOptOrError<RFA_Message> issueRequestSynchronously(
    HostName const &hostName,
    smbase::UniquePtr<RFA_Message> request) override;

  // Pop up a modal error dialog, returning when it is dismissed.
  void complain(string message);

protected Q_SLOTS:
  // Handlers for RFA_AbstractAsyncMultiClient.
  void on_rfaReplyAvailable(RequestID requestID) NOEXCEPT;
  void on_rfaFailed(HostName hostName, string reason) NOEXCEPT;
};


#endif // EDITOR_RFA_SYNC_MULTI_CLIENT_H
