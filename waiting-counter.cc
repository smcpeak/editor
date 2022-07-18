// waiting-counter.cc
// Code for waiting-counter.h.

#include "waiting-counter.h"           // this module

#include "xassert.h"                   // xassert


int g_waitingCounter = 0;


void adjWaitingCounter(int amt)
{
  g_waitingCounter += amt;
  xassert(g_waitingCounter >= 0);
}


// EOF
