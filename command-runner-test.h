// command-runner-test.h
// header for command-runner-test.cc

#ifndef COMMAND_RUNNER_TEST_H
#define COMMAND_RUNNER_TEST_H

#include "command-runner.h"            // CommandRunner

#include "smbase/refct-serf.h"         // RCSerf
#include "smbase/sm-noexcept.h"        // NOEXCEPT

#include <QEventLoop>


// Test class to interact with CommandRunner.
class CRTester : public QEventLoop {
  Q_OBJECT

public:      // types
  // Which test "protocol", i.e., expected inputs and outputs, are we
  // using?
  enum Protocol {
    P_CAT,
    P_ECHO,
    P_KILL,
    P_KILL_NO_WAIT,
    P_FAILED_START,
  };

public:      // data
  // Runner we are monitoring.
  RCSerf<CommandRunner> m_commandRunner;

  // Protocol in use.
  Protocol m_protocol;

  // Where we are in the planned test sequence of stdout output in
  // 'm_protocol'.
  int m_outputState;

  // And for stderr.
  int m_errorState;

public:      // funcs
  CRTester(CommandRunner *runner, Protocol protocol);
  ~CRTester();

  // Run the event loop until the process terminates.  If it is not
  // running, do not even start the loop, just return 0.
  int exec();

public Q_SLOTS:
  void slot_outputLineReady() NOEXCEPT;
  void slot_errorLineReady() NOEXCEPT;
  void slot_processTerminated() NOEXCEPT;
};


#endif // COMMAND_RUNNER_TEST_H
