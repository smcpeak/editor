// os-wait-timer-test.cc
// Tests for `os-wait-timer` module.

// this dir
#include "unit-tests.h"                // decl for my entry point
#include "os-wait-timer.h"             // module under test

// smbase
#include "smbase/htt.h"                // smbase::HTT_Singleton, HTT_TASK
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // EXPECT_EQ, DIAG

// Qt
#include <QEventLoop>
#include <QTimer>

using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


void test_basics()
{
  {
    HTT_TASK("a");

    // Just creating this sets up the signal reactions.
    OSWaitTimer osWaitTimer;

    {
      HTT_TASK("b");

      // Create a Qt timer that runs for 50 ms, and wait for it.
      QEventLoop loop;
      QTimer timer;
      timer.setSingleShot(true);
      QObject::connect(&timer, &QTimer::timeout,
                       &loop, &QEventLoop::quit);
      timer.start(50 /*ms*/);
      loop.exec();           // Block for ~50 ms.
    }

    {
      HTT_TASK("c");
    }
  }

  /* We should see that, during a/b, we spent 50 ms in OSWaitTimer, from
     which we can conclude that that time was not spent on the CPU.

     This looks like:

       timer              elapsed_ms  r
       -----------------  ----------  -
       root                       62  r
         a                        62
           b                      62
             OSWaitTimer          62
           c                       0
  */
  DIAG(HTT_Singleton::asTableString());
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_os_wait_timer(CmdlineArgsSpan args)
{
  test_basics();
}


// EOF
