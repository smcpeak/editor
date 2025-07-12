// lsp-client.h
// Client for the Language Server Protocol.

#ifndef EDITOR_LSP_CLIENT_H
#define EDITOR_LSP_CLIENT_H

#include "command-runner-fwd.h"        // CommandRunner

#include "smbase/gdvalue-fwd.h"        // gdv::GDValue

#include <string_view>                 // std::string_view
#include <vector>                      // std::vector


// Manage communication with a child process that is an LSP server.
class LSPClient {
private:     // data
  // Object managing byte-level communication with the child.
  CommandRunner &m_child;

  // The ID to use for the next request we send.
  int m_nextRequestID;

private:     // methods
  // Send `data` to the child process stdn.
  void send(std::string const &data);

  // Turn `msg` into a sequence of bytes to send.
  static std::string serializeMessage(gdv::GDValue const &msg);

  // Construct a sequence of bytes to represent a request.
  static std::string makeRequest(
    int id,
    std::string_view method,
    gdv::GDValue const &params);

  // Construct a notification byte sequence.
  static std::string makeNotification(
    std::string_view method,
    gdv::GDValue const &params);

  // Read the next response from the server.
  gdv::GDValue readResponse();

  // Read and print responses until we get one that is a reply to a
  // request with `id`, which is returned.
  gdv::GDValue readResponsesUntil(int id);

public:      // methods
  ~LSPClient();

  // Begin communicating using LSP with `child`, a process that has
  // already been started.
  explicit LSPClient(CommandRunner &child);

  // Given a file name, convert that into a "file:" URI.
  static std::string makeFileURI(std::string_view fname);

  // Send a request for `method` with `params`.  Synchronously print all
  // responses up to and including the reply to that request.  Also
  // return the reply.
  gdv::GDValue sendRequestPrintReply(
    std::string_view method,
    gdv::GDValue const &params);

  // Send a notification.  Do not wait for any responses.
  void sendNotification(
    std::string_view method,
    gdv::GDValue const &params);
};


#endif // EDITOR_LSP_CLIENT_H
