// fasttime.cc
// code for fasttime.h

#include "fasttime.h"                  // this module

volatile unsigned fastTimeMilliseconds = 0;

volatile unsigned fastTimeThread1000Loops = 0;


#ifdef __MINGW32__

#include <stdlib.h>                    // abort
#include <process.h>                   // _beginthreadex
#include <windows.h>                   // Sleep

#include <iostream>                    // std::cout

using namespace std;


// Handle to a semaphore used to wait for the thread to start.
static HANDLE hSemaphore = 0;


static unsigned __stdcall fastTimeThreadFunc(void*)
{
  // Do an initial query before incrementing the semaphore.
  fastTimeMilliseconds = GetTickCount();

  // Now let the main thread continue.
  if (!ReleaseSemaphore(hSemaphore, 1 /*count*/, NULL /*prevCount*/)) {
    // I would like to avoid calls into the C++ library in this thread,
    // but this is probably better than being completely silent.
    cout << "fasttime: ReleaseSemaphore failed!" << endl;
    abort();
  }

  unsigned loops = 0;
  while (true) {
    // Keep track of how many times this loop runs in order to ensure
    // it isn't consuming excessive resources.  The shared counter
    // variable is only updated occasionally in order to limit
    // cross-CPU memory traffic.
    loops++;
    if (loops % 1000 == 0) {
      fastTimeThread1000Loops++;
    }

    // Wait a bit.
    Sleep(1 /*ms*/);

    // Update the time count.
    fastTimeMilliseconds = GetTickCount();
  }
}


void fastTimeInitialize()
{
  // Don't start more than one thread.
  static bool started = false;
  if (started) {
    return;
  }
  started = true;

  // Create the semaphore with initial value 0.
  hSemaphore = CreateSemaphore(NULL, 0, 1, NULL);

  // Start the time update thread.
  if (!_beginthreadex(NULL /*security*/,
                      0 /*stackSize*/,
                      &fastTimeThreadFunc,
                      NULL /*argList*/,
                      0 /*initFlag*/,
                      NULL /*thrdAddr*/)) {
    cout << "fasttime: _beginthreadex failed!" << endl;
    abort();
  }

  // Wait for the semaphore to be incremented by the thread.
  DWORD res = WaitForSingleObject(hSemaphore, 1000 /*ms*/);
  if (res != WAIT_OBJECT_0) {
    cout << "fasttime: WaitForSingleObject failed!" << endl;
    abort();
  }
}


#else // !__MINGW32__

// For now, on unix, the "time" will always be zero.
void fastTimeInitialize()
{}

#endif


// EOF
