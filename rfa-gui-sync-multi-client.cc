// rfa-gui-sync-multi-client.cc
// Code for `rfa-gui-sync-multi-client` module.

#include "rfa-gui-sync-multi-client.h" // this module


RFA_GUISyncMultiClient::RFA_GUISyncMultiClient(
  QWidget *widget,
  RFA_AsyncMultiClient *rfaAMClient)
  : GUISynchronousWaiter(widget),
    RFA_SyncMultiClient(rfaAMClient, *this)
{}


// EOF
