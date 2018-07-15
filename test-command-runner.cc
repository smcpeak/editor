// test-command-runner.cc
// Tests for 'command-runner' module.

#include "command-runner.h"            // module to test

// editor
#include "test-command-runner.h"       // helper classes for this module

// smqtutil
#include "qtutil.h"                    // toString
#include "timer-event-loop.h"          // sleepWhilePumpingEvents

// smbase
#include "datablok.h"                  // DataBlock
#include "datetime.h"                  // getCurrentUnixTime
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // replace
#include "test.h"                      // ARGS_MAIN
#include "trace.h"                     // TRACE_ARGS, EXPECT_EQ

// Qt
#include <QCoreApplication>
#include <QProcessEnvironment>

// libc
#include <assert.h>                    // assert


// ----------------------- test infrastructure ----------------------------
// When true, print the byte arrays like a hexdump.
static bool const printByteArrays = false;


static void printCmdArgs(char const *cmd, QStringList const &args)
{
  cout << "run: " << cmd;
  if (!args.isEmpty()) {
    cout << ' ' << toString(args.join(' '));
  }
  cout << endl;
}


static int runCmdArgsIn(char const *cmd, QStringList args, char const *input)
{
  printCmdArgs(cmd, args);
  cout << "  input: \"" << input << "\"" << endl;

  CommandRunner cr;
  cr.setProgram(cmd);
  cr.setArguments(args);

  QByteArray in(input);
  if (printByteArrays && !in.isEmpty()) {
    printQByteArray(in, "stdin");
  }
  cr.setInputData(in);

  cr.startAndWait();

  QByteArray out(cr.getOutputData());
  cout << "  stdout: \"" << out.constData() << "\"" << endl;

  if (printByteArrays && !out.isEmpty()) {
    printQByteArray(out, "stdout");
  }

  QByteArray err(cr.getErrorData());
  cout << "  stderr: \"" << err.constData() << "\"" << endl;

  if (printByteArrays && !err.isEmpty()) {
    printQByteArray(err, "stderr");
  }

  if (cr.getFailed()) {
    cout << "  failed: " << toString(cr.getErrorMessage()) << endl;
    return -1;
  }
  else {
    cout << "  exit code: " << cr.getExitCode() << endl;
    return cr.getExitCode();
  }
}

static int runCmdIn(char const *cmd, char const *input)
{
  return runCmdArgsIn(cmd, QStringList(), input);
}


static void expectEq(char const *label, QByteArray const &actual, char const *expect)
{
  QByteArray expectBA(expect);
  if (actual != expectBA) {
    cout << "mismatched " << label << ':' << endl;
    cout << "  actual: " << actual.constData() << endl;
    cout << "  expect: " << expect << endl;
    xfailure(stringb("mismatched " << label));
  }
  else {
    cout << "  as expected, " << label << ": \""
         << actual.constData() << "\"" << endl;
  }
}


static void runCmdArgsExpectError(char const *cmd,
  QStringList const &args, QProcess::ProcessError error)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), true);
  EXPECT_EQ(cr.getProcessError(), error);
  cout << "  as expected: " << toString(cr.getErrorMessage()) << endl;
}


static void runCmdExpectError(char const *cmd, QProcess::ProcessError error)
{
  runCmdArgsExpectError(cmd, QStringList(), error);
}


static void runCmdArgsExpectExit(char const *cmd,
  QStringList const &args, int exitCode)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getExitCode(), exitCode);
  cout << "  as expected: exit " << cr.getExitCode() << endl;
}


static void runCmdExpectExit(char const *cmd, int exitCode)
{
  runCmdArgsExpectExit(cmd, QStringList(), exitCode);
}


static void runCmdArgsInExpectOut(char const *cmd,
  QStringList const &args, char const *input, char const *output)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.setInputData(QByteArray(input));
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData(), output);
}


static void runCmdArgsExpectOutErr(char const *cmd,
  QStringList const &args, char const *output, char const *error)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData(), output);
  EXPECT_EQ(cr.getErrorData(), error);
}


// Normalize a string that represents a directory path prior to
// comparing it to an expected value.
static string normalizeDir(string d)
{
  if (SMFileUtil().windowsPathSemantics()) {
    d = replace(d, "\\", "/");
    d = translate(d, "A-Z", "a-z");
    if (d.length() >= 12 &&
        d.substring(0, 10) == "/cygdrive/" &&
        d[11] == '/')
    {
      char letter = d[10];
      d = stringb(letter << ":/" << d.substring(12, d.length()-12));
    }
  }

  // Paths can have whitespace at either end, but rarely do, and I
  // need to discard the newline that 'pwd' prints.
  d = trimWhitespace(d);

  return d;
}


static void runCmdDirExpectOutDir(string const &cmd,
  string const &wd, string const &expectDir)
{
  cout << "run: cmd=" << cmd << " wd=" << wd << endl;
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  if (!wd.isempty()) {
    cr.setWorkingDirectory(toQString(wd));
  }
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);

  string actualDir = cr.getOutputData().constData();

  string expectNormDir = normalizeDir(expectDir);
  string actualNormDir = normalizeDir(actualDir);
  EXPECT_EQ(actualNormDir, expectNormDir);
  cout << "  as expected, got dir: " << trimWhitespace(actualDir) << endl;
}


static void runCmdExpectOutDir(string const &cmd, string const &output)
{
  runCmdDirExpectOutDir(cmd, "", output);
}


// ----------------------------- tests ---------------------------------
static void testProcessError()
{
  runCmdExpectError("nonexistent-command", QProcess::FailedToStart);
  runCmdArgsExpectError("sleep", QStringList() << "3", QProcess::Timedout);

  // Test that the timeout allows a 1s program to terminate.
  runCmdArgsExpectExit("sleep", QStringList() << "1", 0);
}


static void testExitCode()
{
  runCmdExpectExit("true", 0);
  runCmdExpectExit("false", 1);
  runCmdArgsExpectExit("perl", QStringList() << "-e" << "exit(42);", 42);
}


static void testOutputData()
{
  runCmdArgsInExpectOut("tr", QStringList() << "a-z" << "A-Z",
    "hello", "HELLO");
  runCmdArgsInExpectOut("tr", QStringList() << "a-z" << "A-Z",
    "one\ntwo\nthree\n", "ONE\nTWO\nTHREE\n");

  runCmdArgsExpectOutErr("sh",
    QStringList() << "-c" << "echo -n to stdout ; echo -n to stderr 1>&2",
    "to stdout", "to stderr");
}


static void testLargeData1()
{
  cout << "testing cat on 100kB..." << endl;

  QByteArray input;
  for (int i=0; i < 100000; i++) {
    input.append((char)i);
  }

  CommandRunner cr;
  cr.setProgram("cat");
  cr.setInputData(input);
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData().size(), input.size());
  xassert(cr.getOutputData() == input);

  cout << "  cat 100kB worked" << endl;
}


static void testLargeData2(bool swapOrder)
{
  cout << "testing cat on source code..." << endl;

  DataBlock outputDB;
  outputDB.readFromFile("editor-widget.cc");
  QByteArray output((char const *)outputDB.getData(), outputDB.getDataLen());

  DataBlock errorDB;
  errorDB.readFromFile("test-td-editor.cc");
  QByteArray error((char const *)errorDB.getData(), errorDB.getDataLen());

  CommandRunner cr;
  cr.setProgram("sh");

  // In my testing on Windows with cygwin sh, swapping the order of
  // commands in this pipeline does alter the order of events received
  // by my program, so it is good to test both ways.
  cr.setArguments(QStringList() << "-c" <<
    (swapOrder?
      "(cat test-td-editor.cc 1>&2) & cat editor-widget.cc ; wait $!" :
      "cat editor-widget.cc & (cat test-td-editor.cc 1>&2) ; wait $!"));

  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData().size(), output.size());
  xassert(cr.getOutputData() == output);
  EXPECT_EQ(cr.getErrorData().size(), error.size());
  xassert(cr.getErrorData() == error);

  cout << "  cat of source code worked" << endl;
}


static void testWorkingDirectory()
{
  string cwd = SMFileUtil().currentDirectory();

  runCmdExpectOutDir("pwd", cwd);
  runCmdDirExpectOutDir("pwd", ".", cwd);

  string testDir = stringb(cwd << "/test");
  runCmdDirExpectOutDir("pwd", testDir, testDir);
  runCmdDirExpectOutDir("pwd", "test", testDir);

  string testDirA = stringb(cwd << "/test/a");
  runCmdDirExpectOutDir("pwd", testDirA, testDirA);
  runCmdDirExpectOutDir("pwd", "test/a", testDirA);

#ifdef __WIN32__
  runCmdDirExpectOutDir("pwd", "c:/", "/cygdrive/c");
  runCmdDirExpectOutDir("pwd", "c:/windows", "/cygdrive/c/windows");
#endif
}


// These aren't tests per se, just things that can be helpful to inspect.
static void testMiscDiagnostics()
{
  runCmdArgsIn("cmd", QStringList() << "/c" << "echo %PATH%", "");
  runCmdArgsIn("cmd", QStringList() << "/c" << "set", "");
  runCmdArgsIn("cmd", QStringList() << "/c" << "sort /?", "");

  // If PATH search finds Windows sort, it thinks this input is some
  // multibyte encoding and produces "??????" as output.
  runCmdIn("sort", "a\nc\nb\nz\ny\n1\n");
  runCmdIn("sort", "a\nc\nb\nz\ny\n12\n");
}


static void sleepBriefly()
{
  sleepWhilePumpingEvents(200 /*ms*/);
}


// Running a program asynchronously and not using any signals, just
// waiting and polling.
static void testAsyncNoSignals()
{
  CommandRunner cr;
  cr.setProgram("cat");
  cr.startAsynchronous();

  sleepBriefly();
  xassert(cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());

  cr.putInputData(QByteArray("hello\n"));
  sleepBriefly();
  xassert(cr.isRunning());
  xassert(cr.hasOutputData());
  xassert(!cr.hasErrorData());
  QByteArray output = cr.takeOutputData();
  xassert(output == QByteArray("hello\n"));

  cr.putInputData(QByteArray("this is a second line\n"));
  sleepBriefly();
  xassert(cr.isRunning());
  xassert(cr.hasOutputData());
  xassert(!cr.hasErrorData());
  output = cr.takeOutputData();
  xassert(output == QByteArray("this is a second line\n"));

  cr.closeInputChannel();
  sleepBriefly();
  xassert(!cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());
  xassert(!cr.getFailed());
  xassert(cr.getExitCode() == 0);
}


CRTester::CRTester(CommandRunner *runner, Protocol protocol)
  : QEventLoop(),
    m_commandRunner(runner),
    m_protocol(protocol),
    m_outputState(0),
    m_errorState(0)
{
  QObject::connect(runner, &CommandRunner::signal_outputLineReady,
                   this,          &CRTester::slot_outputLineReady);
  QObject::connect(runner, &CommandRunner::signal_errorLineReady,
                   this,          &CRTester::slot_errorLineReady);
  QObject::connect(runner, &CommandRunner::signal_processTerminated,
                   this,          &CRTester::slot_processTerminated);
}

CRTester::~CRTester()
{}


void CRTester::slot_outputLineReady() NOEXCEPT
{
  while (m_commandRunner->hasOutputLine()) {
    QString line = m_commandRunner->getOutputLine();
    switch (m_protocol) {
      case P_CAT:
        switch (m_outputState) {
          case 0:
            assert(line == "hello\n");
            m_commandRunner->putInputData("second line\n");
            m_outputState++;
            break;

          case 1:
            assert(line == "second line\n");
            m_commandRunner->closeInputChannel();
            m_outputState++;
            break;

          default:
            assert("!not state 1 or 0");
            break;
        }
        break;

      case P_ECHO:
        switch (m_outputState) {
          case 0:
            assert(line == "stdout1\n");
            m_commandRunner->putInputData("dummy value\n");
            m_outputState++;
            break;

          case 1:
            assert(line == "stdout2\n");
            m_outputState++;
            break;

          default:
            assert(!"bad state");
            break;
        }
        break;

      case P_KILL:
        switch (m_outputState) {
          case 0:
            assert(line == "hello\n");
            m_commandRunner->killProcess();
            m_outputState++;
            break;

          default:
            assert(!"bad state");
            break;
        }
        break;

      case P_KILL_NO_WAIT:
        switch (m_outputState) {
          case 0:
            assert(line == "hello\n");
            m_commandRunner->killProcessNoWait();
            m_outputState++;

            // Quit the event loop so I can see the dtor complaint.
            this->exit(0);

            break;

          default:
            assert(!"bad state");
            break;
        }
        break;

      default:
        assert(!"bad protocol");
        break;
    }
  }
}


void CRTester::slot_errorLineReady() NOEXCEPT
{
  while (m_commandRunner->hasErrorLine()) {
    QString line = m_commandRunner->getErrorLine();
    switch (m_protocol) {
      case P_CAT:
      case P_KILL:
      case P_KILL_NO_WAIT:
        assert(!"should not be any error data");
        break;

      case P_ECHO:
        switch (m_errorState) {
          case 0:
            assert(line == "stderr1\n");
            m_errorState++;
            break;

          case 1:
            assert(line == "stderr2\n");
            m_errorState++;
            break;

          default:
            assert(!"bad state");
            break;
        }
        break;

      default:
        assert(!"bad protocol");
        break;
    }
  }
}


void CRTester::slot_processTerminated() NOEXCEPT
{
  if (m_protocol != P_KILL && m_protocol != P_KILL_NO_WAIT) {
    assert(!m_commandRunner->isRunning());
    assert(!m_commandRunner->getFailed());
    assert(m_commandRunner->getExitCode() == 0);
  }

  // Terminate the event loop.
  this->exit(0);
}


static void testAsyncWithSignals()
{
  CommandRunner cr;
  CRTester tester(&cr, CRTester::P_CAT);

  cr.setProgram("cat");
  cr.startAsynchronous();

  cr.putInputData(QByteArray("hello\n"));

  // Run the event loop until the test finishes.
  tester.exec();

  // This is partially redundant with tests in
  // CRTester::slot_processTerminated, but that's ok.
  xassert(!cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());
  xassert(!cr.getFailed());
  xassert(cr.getExitCode() == 0);
}


static void testAsyncBothOutputs()
{
  CommandRunner cr;
  CRTester tester(&cr, CRTester::P_ECHO);

  cr.setProgram("sh");
  cr.setArguments(QStringList() << "-c" <<
    "echo stdout1; echo stderr1 1>&2; read dummy; "
    "echo stdout2; echo stderr2 1>&2");
  cr.startAsynchronous();

  tester.exec();

  xassert(!cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());
  xassert(!cr.getFailed());
  xassert(cr.getExitCode() == 0);
}


static void testAsyncKill(bool wait)
{
  try {
    CommandRunner cr;
    CRTester tester(&cr, wait? CRTester::P_KILL : CRTester::P_KILL_NO_WAIT);

    cr.setProgram("cat");
    cr.startAsynchronous();

    cr.putInputData(QByteArray("hello\n"));

    int res = tester.exec();
    cout << "testAsyncKill: exec() finished with code " << res << endl;

    if (wait) {
      xassert(!cr.isRunning());
      xassert(!cr.hasOutputData());
      xassert(!cr.hasErrorData());

      xassert(cr.getFailed());
      PVAL(toString(cr.getErrorMessage()));
      xassert(cr.getProcessError() == QProcess::Crashed);
    }
    else {
      // Since we didn't give the event loop an opportunity to run,
      // QProcess should still think the process is alive.
      xassert(cr.isRunning());
    }
  }
  catch (...) {
    cout << "testAsyncKill: exception propagating out" << endl;
    throw;
  }

  cout << "testAsyncKill(" << wait << ") finished" << endl;
}


static void expectSSCL(char const *input, char const *expect)
{
  CommandRunner r;
  r.setShellCommandLine(input);
  string actual = toString(r.getCommandLine());
  EXPECT_EQ(actual, string(expect));
}

static void testSetShellCommandLine()
{
  expectSSCL("date", "date");
  expectSSCL("echo hi", "echo hi");
  expectSSCL("date | date", "sh -c date | date");
  expectSSCL("echo 'hi'", "sh -c echo 'hi'");
}


static void printStatus(CommandRunner &runner)
{
  cout << "CommandRunner running: " << runner.isRunning() << endl;
  if (!runner.isRunning()) {
    cout << "CommandRunner failed: " << runner.getFailed() << endl;
    if (runner.getFailed()) {
      cout << "CommandRunner error: "
           << toString(runner.getProcessError()) << endl;
      cout << "CommandRunner error message: "
           << toString(runner.getErrorMessage()) << endl;
    }
    else {
      cout << "CommandRunner exit code: "
           << runner.getExitCode() << endl;
    }
  }
}

// Run a program and then kill it.
//
// Adapted from wrk/learn/qt5/qproc.cc.
static void runAndKill(int argc, char **argv)
{
  UnixTime startTime = 0;

  {
    CommandRunner runner;
    runner.setProgram(argv[1]);
    QStringList args;
    for (int i=2; i < argc; i++) {
      args << argv[i];
    }
    runner.setArguments(args);

    // Child will inherit stdin/out/err.
    runner.forwardChannels();

    cout << "starting: " << argv[1] << (args.isEmpty()? "" : " ")
         << args.join(' ').toUtf8().constData() << endl;
    runner.startAsynchronous();

    // Wait a moment to reach quiescence.
    cout << "waiting for 200 ms ..." << endl;
    sleepWhilePumpingEvents(200);
    printStatus(runner);

    // Attempt to kill the process.
    cout << "calling killProcessNoWait ..." << endl;
    runner.killProcessNoWait();

    // Wait again.
    cout << "waiting for 200 ms ..." << endl;
    sleepWhilePumpingEvents(200);
    printStatus(runner);

    // Now let the destructor run.
    startTime = getCurrentUnixTime();
    cout << "destroying CommandRunner ..." << endl;
  }

  cout << "CommandRunner destructor took about "
       << (getCurrentUnixTime() - startTime) << " seconds" << endl;
}


#define RUN(statement)                                         \
  if (!oneTest || 0==strcmp(oneTest, #statement)) {            \
    cout << "------ " << #statement << " ------" << endl;      \
    statement;                                                 \
  }


static void entry(int argc, char **argv)
{
  TRACE_ARGS();

  QCoreApplication app(argc, argv);

  if (argc >= 2) {
    // Special mode for interactive testing of CommandRunner.
    runAndKill(argc, argv);
    return;
  }

  // Cygwin is needed for the build anyway, so this should not be a
  // big deal.  I did give some thought to writing the tests so they
  // work work without cygwin, but plain Windows is a very spartan
  // environment.
  if (SMFileUtil().windowsPathSemantics()) {
    cout << "NOTE: These tests require cygwin on Windows.\n";
  }

  // Optionally run just one test.
  char const *oneTest = getenv("TEST_CMD_ONE");

  RUN(testProcessError());
  RUN(testExitCode());
  RUN(testOutputData());
  RUN(testLargeData1());
  RUN(testLargeData2(false));
  RUN(testLargeData2(true));
  RUN(testWorkingDirectory());
  RUN(testAsyncNoSignals());
  RUN(testAsyncWithSignals());
  RUN(testAsyncBothOutputs());
  RUN(testAsyncKill(true));
  RUN(testAsyncKill(false));
  RUN(testMiscDiagnostics());
  RUN(testSetShellCommandLine());

  cout << "test-command-runner tests passed" << endl;
}

ARGS_TEST_MAIN


// EOF
