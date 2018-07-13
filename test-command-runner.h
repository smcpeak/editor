// test-command-runner.h
// header for test-command-runner.cc

#ifndef TEST_COMMAND_RUNNER_H
#define TEST_COMMAND_RUNNER_H

#include "command-runner.h"            // CommandRunner

#include "refct-serf.h"                // RCSerf
#include "sm-noexcept.h"               // NOEXCEPT

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

public Q_SLOTS:
  void slot_outputLineReady() NOEXCEPT;
  void slot_errorLineReady() NOEXCEPT;
  void slot_processTerminated() NOEXCEPT;
};


#endif // TEST_COMMAND_RUNNER_H
