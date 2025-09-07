// json-rpc-client-test.h
// `JSON_RPC_ClientTester` class.

// This has to be in its own file (rather than in `json-rpc-client-test.cc`)
// in order to be able to run `moc` on it, which is in turn necessary
// because the class inherits `QObject`.

#ifndef EDITOR_LSP_CLIENT_TEST_H
#define EDITOR_LSP_CLIENT_TEST_H

#include "json-rpc-client.h"                     // JSON_RPC_Client, JSON_RPC_Error
#include "lsp-test-request-params.h"             // LSPTestRequestParams

#include "smbase/either.h"                       // smbase::Either
#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/std-string-view-fwd.h"          // std::string_view

#include <QObject>

#include <optional>                              // std::optional
#include <string>                                // std::string


// Class to receive signals from `JSON_RPC_Client`.
class JSON_RPC_ClientTester : public QObject {
  Q_OBJECT;

public:      // data
  // Client interface we're connected to.
  JSON_RPC_Client &m_lsp;

  // Request details derived from the command line.
  LSPTestRequestParams m_params;

  // True once we have initiated client shutdown.  If the client
  // terminates before this is set, that is an error.
  bool m_initiatedShutdown;

  // Set once we've completed the test with this object.
  bool m_done;

  // Set if we finished due to an error.
  std::optional<std::string> m_failureMsg;

public:      // methods
  ~JSON_RPC_ClientTester();

  JSON_RPC_ClientTester(
    JSON_RPC_Client &lsp,
    LSPTestRequestParams const &params);

  // Send a request and check that its assigned ID is as expected.
  void sendRequestCheckID(
    int expectID,
    std::string_view method,
    gdv::GDValue const &params);

  // Given that we have just received `prevReply` for `prevID` (where 0
  // and nullopt means we're just starting), send the next request, or
  // else set `m_done` if we are done.
  void sendNextRequest(
    int prevID, std::optional<JSON_RPC_Reply> const &prevReply);

  // Set `m_failureMsg` and `m_done`, and print the mssage to stdout.
  // However, if `m_failureMsg` is already set, then ignore this.
  void setFailureMsg(std::string &&msg);

public Q_SLOTS:
  void on_hasPendingNotifications() NOEXCEPT;
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasProtocolError() NOEXCEPT;
  void on_childProcessTerminated() NOEXCEPT;
};


#endif // EDITOR_LSP_CLIENT_TEST_H
