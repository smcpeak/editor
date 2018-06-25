// inputproxy.cc
// code for inputproxy.h

#include "inputproxy.h"      // this module

#include "editor-widget.h"   // EditorWidget
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


bool InputProxy::pseudoKeyPress(InputPseudoKey pkey)
{
  return false;    // not handled
}


// ---------------- AttachInputProxy -----------------
AttachInputProxy::AttachInputProxy()
  : ed(NULL)
{}

AttachInputProxy::~AttachInputProxy()
{
  detach();
}


void AttachInputProxy::attach(EditorWidget *newEd)
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
