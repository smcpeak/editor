// lsp-command-runner.h
// `LSPCommandRunner` class.

#ifndef EDITOR_LSP_COMMAND_RUNNER_H
#define EDITOR_LSP_COMMAND_RUNNER_H

#include "command-runner.h"            // CommandRunner
#include "lsp-client.h"                // LSPClient


// Package a `CommandRunner` and an `LSPClient` together.
//
// This doesn't encapsulate the objects, it just puts them next to each
// other for convenience.  Clients are expected to directly access them,
// including connecting their signals to slots of the client.
//
class LSPCommandRunner {
public:      // data
  // Object to manage the child process.
  CommandRunner m_commandRunner;

  // Protocol communicator.
  LSPClient m_lsp;

public:
  ~LSPCommandRunner();

  // Configures `m_commandRunner` to run `clangd`, but does not start
  // it.
  LSPCommandRunner();

  void startServer();

  bool serverStarted() const;




#endif // EDITOR_LSP_COMMAND_RUNNER_H
