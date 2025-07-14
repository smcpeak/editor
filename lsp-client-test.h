// lsp-client-test.h
// `LSPClientTester` class.

// This has to be in its own file (rather than in `lsp-client-test.cc`)
// in order to be able to run `moc` on it, which is in turn necessary
// because the class inherits `QObject`.

#ifndef EDITOR_LSP_CLIENT_TEST_H
#define EDITOR_LSP_CLIENT_TEST_H

#include "lsp-client-fwd.h"                      // LSPClient

#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue
#include "smbase/sm-noexcept.h"                  // NOEXCEPT
#include "smbase/std-string-view-fwd.h"          // std::string_view

#include <QObject>

#include <string>                                // std::string


// Class to receive signals from `LSPClient`.
class LSPClientTester : public QObject {
  Q_OBJECT;

public:      // data
  // Client interface we're connected to.
  LSPClient &m_lsp;

  // Request details derived from the command line.
  std::string const &m_fname;
  int m_line;
  int m_col;
  std::string const &m_fileContents;

  // Set once we've completed the test with this object.
  bool m_done;

  // Set if we finished due to an error.
  bool m_failed;

public:      // methods
  ~LSPClientTester();

  LSPClientTester(
    LSPClient &lsp,
    std::string const &fname,
    int line,
    int col,
    std::string const &fileContents);

  // Send a request and check that its assigned ID is as expected.
  void sendRequestCheckID(
    int expectID,
    std::string_view method,
    gdv::GDValue const &params);

  // Given that we have just received the reply for `prevID` (where 0
  // means we're just starting), send the next request, or else set
  // `m_done` if we are done.
  void sendNextRequest(int prevID);

public Q_SLOTS:
  void on_hasPendingNotifications() NOEXCEPT;
  void on_hasReplyForID(int id) NOEXCEPT;
  void on_hasProtocolError() NOEXCEPT;
  void on_childProcessTerminated() NOEXCEPT;
};


#endif // EDITOR_LSP_CLIENT_TEST_H
