// inputproxy.cc
// code for inputproxy.h

#include "inputproxy.h"      // this module
#include "editor.h"          // Editor
#include "qevent.h"          // QKeyEvent

#include <stddef.h>          // NULL


// ------------------ InputProxy ---------------------
InputProxy::~InputProxy()
{}


bool InputProxy::keyPressEvent(QKeyEvent *k)
{
  return false;    // not handled
}


void InputProxy::detach()
{}


// ---------------- AttachInputProxy -----------------
AttachInputProxy::AttachInputProxy()
  : ed(NULL)
{}

AttachInputProxy::~AttachInputProxy()
{
  detach();
}


void AttachInputProxy::attach(Editor *newEd)
{
  if (ed) {
    detach();
  }

  ed = newEd;
  ed->inputProxy = this;
}

void AttachInputProxy::detach()
{                       
  if (ed) {
    ed->inputProxyDetaching();
    ed->inputProxy = NULL;
    ed = NULL;
  }
}
