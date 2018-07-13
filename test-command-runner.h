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

public:      // data
  // Runner we are monitoring.
  RCSerf<CommandRunner> m_commandRunner;

  // Where we are in the planned test sequence.
  int m_state;

public:      // funcs
  CRTester(CommandRunner *runner);
  ~CRTester();

public Q_SLOTS:
  void slot_outputLineReady() NOEXCEPT;
  void slot_errorLineReady() NOEXCEPT;
  void slot_processTerminated() NOEXCEPT;
};


#endif // TEST_COMMAND_RUNNER_H
