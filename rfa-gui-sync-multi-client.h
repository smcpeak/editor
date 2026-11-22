// rfa-gui-sync-multi-client.h
// `RFA_GUISyncMultiClient` class

// See license.txt for copyright and terms of use.

#ifndef EDITOR_RFA_GUI_SYNC_MULTI_CLIENT_H
#define EDITOR_RFA_GUI_SYNC_MULTI_CLIENT_H

// smqtutil
#include "smqtutil/sync-wait.h"                  // GUISynchronousWaiter

// editor
#include "rfa-async-multi-client-fwd.h"          // RFA_AsyncMultiClient [f]
#include "rfa-sync-multi-client.h"               // RFA_SyncMultiClient

class QWidget;


/*
  Encapsulate the `GUISynchronousWaiter` plus `RFA_SyncMultiClient`
  combination.
*/
class RFA_GUISyncMultiClient : public GUISynchronousWaiter,
                               public RFA_SyncMultiClient {
public:      // funcs
  // Create a synchronous client that will put its cancelable progress
  // dialog on top of `widget` and use `rfaAMClient` for RFA.
  RFA_GUISyncMultiClient(
    QWidget *widget,
    RFA_AsyncMultiClient *rfaAMClient);
};


#endif // EDITOR_RFA_GUI_SYNC_MULTI_CLIENT_H
