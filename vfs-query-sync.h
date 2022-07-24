// vfs-query-sync.h
// VFS_QuerySync class.

#ifndef EDITOR_VFS_QUERY_SYNC_H
#define EDITOR_VFS_QUERY_SYNC_H

// editor
#include "host-name.h"                 // HostName
#include "vfs-connections.h"           // VFS_Connections

// qt
#include <QObject>
#include <QEventLoop>
#include <QTimer>

class QWidget;


// Like FileSystemQuery, but with a synchronous interface and an
// implementation that has a GUI to allow the user to cancel requests.
class VFS_QuerySync : public QObject {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_QuerySync);

public:      // types
  typedef VFS_Connections::RequestID RequestID;

private:     // data
  // Query interface to use.
  VFS_Connections *m_vfsConnections;

  // Host to access.
  HostName m_hostName;

  // Parent widget to modally interrupt if needed.
  QWidget *m_parentWidget;

  // ID of outstanding request, or 0 if none.
  RequestID m_requestID;

  // Reply, if one has arrived.
  std::unique_ptr<VFS_Message> m_reply;

  // Connection lost message, or empty string.
  string m_connLostMessage;

  // Event loop to allow waiting.
  QEventLoop m_eventLoop;

  // Timer object to wake up after a certain delay.
  QTimer m_timer;

public:      // methods
  // Create an object to issue queries to 'hostName' via
  // 'vfsConnections', and if needed, pop up a modal window on top of
  // 'parentWidget'.
  VFS_QuerySync(VFS_Connections *vfsConnections,
    HostName const &hostName, QWidget *parentWiget);
  virtual ~VFS_QuerySync() override;

  // Issue 'request' and wait for the reply.  If a reply arrives, set
  // 'reply' to it and return true.  If the connection is lost, set
  // 'connLostMessage' to the error message and return true.  If the
  // user cancels the request, return false.
  bool issueRequestSynchronously(
    std::unique_ptr<VFS_Message> request,
    std::unique_ptr<VFS_Message> /*OUT*/ &reply,
    string /*OUT*/ &connLostMessage);

protected Q_SLOTS:
  // Handlers for VFS_Connections.
  void on_replyAvailable(RequestID requestID) NOEXCEPT;
  void on_vfsConnectionLost(HostName hostName, string reason) NOEXCEPT;

  // Handlers for QTimer.
  void on_timeout() NOEXCEPT;

  // Handlers for QProgressDialog.
  void on_canceled() NOEXCEPT;
};


#endif // EDITOR_VFS_QUERY_SYNC_H
