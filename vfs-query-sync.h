// vfs-query-sync.h
// VFS_QuerySync class.

#ifndef EDITOR_VFS_QUERY_SYNC_H
#define EDITOR_VFS_QUERY_SYNC_H

// editor
#include "host-and-resource-name.h"    // HostAndResourceName
#include "host-name.h"                 // HostName
#include "nearby-file.h"               // IHFExists
#include "vfs-connections.h"           // VFS_Connections

// qt
#include <QObject>
#include <QEventLoop>
#include <QTimer>

class QWidget;


// Like VFS_FileSystemQuery, but with a synchronous interface and an
// implementation that has a GUI to allow the user to cancel requests.
class VFS_QuerySync : public QObject,
                      public IHFExists {
  Q_OBJECT
  NO_OBJECT_COPIES(VFS_QuerySync);

public:      // types
  typedef VFS_Connections::RequestID RequestID;

private:     // data
  // Query interface to use.
  VFS_Connections *m_vfsConnections;

  // Parent widget to modally interrupt if needed.
  QWidget *m_parentWidget;

  // ID of outstanding request, or 0 if none.
  RequestID m_requestID;

  // If 'm_requestID' is not zero, this is the host being queried.
  // Otherwise, it is meaningless.
  HostName m_hostName;

  // Reply, if one has arrived.
  std::unique_ptr<VFS_Message> m_reply;

  // Connection lost message, or empty string.
  string m_connLostMessage;

  // Event loop to allow waiting.
  QEventLoop m_eventLoop;

  // Timer object to wake up after a certain delay.
  QTimer m_timer;

public:      // instance methods
  // Create an object to issue queries via 'vfsConnections', and if
  // needed, pop up a modal window on top of 'parentWidget'.
  VFS_QuerySync(VFS_Connections *vfsConnections, QWidget *parentWiget);
  virtual ~VFS_QuerySync() override;

  // Issue 'request' and wait for the reply.  If a reply arrives, set
  // 'reply' to it and return true.  If the connection is lost, set
  // 'connLostMessage' to the error message and return true.  If the
  // user cancels the request, return false.
  bool issueRequestSynchronously(
    HostName const &hostName,
    std::unique_ptr<VFS_Message> request,
    std::unique_ptr<VFS_Message> /*OUT*/ &reply,
    string /*OUT*/ &connLostMessage);

  // Issue 'request' synchronously, expecting to get 'REPLY_TYPE'.
  //
  // If there is an error, including if 'hostName' is invalid, pop up an
  // error box and return an empty pointer.
  template <class REPLY_TYPE>
  std::unique_ptr<REPLY_TYPE> issueTypedRequestSynchronously(
    HostName const &hostName,
    std::unique_ptr<VFS_Message> request);

  // Pop up a modal error dialog, returning when it is dismissed.
  void complain(string message);

  // IHFExists methods.
  virtual bool hfExists(HostAndResourceName const &harn) override;

protected Q_SLOTS:
  // Handlers for VFS_Connections.
  void on_vfsReplyAvailable(RequestID requestID) NOEXCEPT;
  void on_failed(HostName hostName, string reason) NOEXCEPT;

  // Handlers for QTimer.
  void on_timeout() NOEXCEPT;

  // Handlers for QProgressDialog.
  void on_canceled() NOEXCEPT;
};


template <class REPLY_TYPE>
std::unique_ptr<REPLY_TYPE> VFS_QuerySync::issueTypedRequestSynchronously(
  HostName const &hostName,
  std::unique_ptr<VFS_Message> request)
{
  // Initially empty pointer, used for error returns.
  std::unique_ptr<REPLY_TYPE> typedReply;

  if (!m_vfsConnections->isValid(hostName)) {
    complain(stringb("Host " << hostName << " is invalid."));
    return typedReply;
  }

  // Issue the request.
  std::unique_ptr<VFS_Message> genericReply;
  string connLostMessage;
  if (!issueRequestSynchronously(
         hostName, std::move(request), genericReply, connLostMessage)) {
    // Request was canceled.
    return typedReply;
  }

  if (!connLostMessage.empty()) {
    this->complain(stringb(
      "VFS connection lost: " << connLostMessage));
    return typedReply;
  }

  if (REPLY_TYPE *r = dynamic_cast<REPLY_TYPE*>(genericReply.get())) {
    // Move the pointer from 'genericReply' to 'typedReply'.
    genericReply.release();
    typedReply.reset(r);
    return typedReply;
  }

  else {
    this->complain(stringb(
      "Server responded with incorrect message type: " <<
      toString(genericReply->messageType())));
    return typedReply;
  }
}


// Read the contents of 'fname', waiting for the reply and blocking user
// input during the wait.
//
// If this returns an empty pointer, then either the user canceled the
// request, or an error was already reported; nothing further needs to
// be done.  But if present, the reply object might itself have
// 'm_success==false', which needs to be handled by the caller.
std::unique_ptr<VFS_ReadFileReply> readFileSynchronously(
  VFS_Connections *vfsConnections,
  QWidget *parentWidget,
  HostAndResourceName const &harn);


// Get timestamp, etc., for 'fname'.
std::unique_ptr<VFS_FileStatusReply> getFileStatusSynchronously(
  VFS_Connections *vfsConnections,
  QWidget *parentWidget,
  HostAndResourceName const &harn);


#endif // EDITOR_VFS_QUERY_SYNC_H
