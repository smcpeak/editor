// command-runner-test.cc
// Tests for 'command-runner' module.

#include "unit-tests.h"                // decl for my entry point

#include "command-runner.h"            // module to test

// editor
#include "command-runner-test.h"       // helper classes for this module

// smqtutil
#include "smqtutil/qtutil.h"           // toString
#include "smqtutil/timer-event-loop.h" // sleepWhilePumpingEvents

// smbase
#include "smbase/datablok.h"           // DataBlock
#include "smbase/datetime.h"           // getCurrentUnixTime
#include "smbase/exc.h"                // xfatal
#include "smbase/nonport.h"            // getMilliseconds
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-macros.h"          // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"            // ARGS_MAIN, VPVAL, DIAG, EXPECT_EQ, verbose
#include "smbase/string-util.h"        // beginsWith, replaceAll
#include "smbase/xassert.h"            // xfailure_stringbc

// Qt
#include <QCoreApplication>
#include <QProcessEnvironment>

// libc
#include <assert.h>                    // assert

using std::cout;


OPEN_ANONYMOUS_NAMESPACE

// I define an `expectEq` for byte arrays below.  We need to explicitly
// import the global scope declarations to have them in the overload
// set.
using ::expectEq;


// ----------------------- test infrastructure ----------------------------
// When true, print the byte arrays like a hexdump.
bool const printByteArrays = false;


void printCmdArgs(char const *cmd, QStringList const &args)
{
  if (!verbose) {
    return;
  }

  cout << "run: " << cmd;
  if (!args.isEmpty()) {
    cout << ' ' << toString(args.join(' '));
  }
  cout << endl;
}


int runCmdArgsIn(char const *cmd, QStringList args, char const *input)
{
  printCmdArgs(cmd, args);
  DIAG("  input: \"" << input << "\"");

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
  DIAG("  stdout: \"" << out.constData() << "\"");

  if (printByteArrays && !out.isEmpty()) {
    printQByteArray(out, "stdout");
  }

  QByteArray err(cr.getErrorData());
  DIAG("  stderr: \"" << err.constData() << "\"");

  if (printByteArrays && !err.isEmpty()) {
    printQByteArray(err, "stderr");
  }

  if (cr.getFailed()) {
    DIAG("  failed: " << toString(cr.getErrorMessage()));
    DIAG("  term desc: " << toString(cr.getTerminationDescription()));
    return -1;
  }
  else {
    DIAG("  exit code: " << cr.getExitCode());
    DIAG("  term desc: " << toString(cr.getTerminationDescription()));
    return cr.getExitCode();
  }
}

int runCmdIn(char const *cmd, char const *input)
{
  return runCmdArgsIn(cmd, QStringList(), input);
}


void expectEq(char const *label, QByteArray const &actual, char const *expect)
{
  QByteArray expectBA(expect);
  if (actual != expectBA) {
    DIAG("mismatched " << label << ':');
    DIAG("  actual: " << actual.constData());
    DIAG("  expect: " << expect);
    xfailure_stringbc("mismatched " << label);
  }
  else {
    DIAG("  as expected, " << label << ": \"" <<
         actual.constData() << "\"");
  }
}


void runCmdArgsExpectError(char const *cmd,
  QStringList const &args, QProcess::ProcessError error)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), true);
  EXPECT_EQ(cr.getProcessError(), error);
  DIAG("  as expected: " << toString(cr.getErrorMessage()));
  DIAG("  term desc: " << toString(cr.getTerminationDescription()));
}


void runCmdExpectError(char const *cmd, QProcess::ProcessError error)
{
  runCmdArgsExpectError(cmd, QStringList(), error);
}


void runCmdArgsExpectExit(char const *cmd,
  QStringList const &args, int exitCode)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getExitCode(), exitCode);
  DIAG("  as expected: exit " << cr.getExitCode());
  DIAG("  term desc: " << toString(cr.getTerminationDescription()));
}


void runCmdExpectExit(char const *cmd, int exitCode)
{
  runCmdArgsExpectExit(cmd, QStringList(), exitCode);
}


void runCmdArgsInExpectOut(char const *cmd,
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


void runCmdArgsExpectOutErr(char const *cmd,
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


// Run the command with stderr redirected to stdout.
void runMergedCmdArgsExpectOut(char const *cmd,
  QStringList const &args, char const *output)
{
  printCmdArgs(cmd, args);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  cr.setArguments(args);
  cr.mergeStderrIntoStdout();
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData(), output);
  EXPECT_EQ(cr.getErrorData(), "");
}


// Run 'cygpath -m' on 'input' and return its result.
string runCygpath(string input)
{
  CommandRunner cr;
  cr.setProgram("cygpath");
  cr.setArguments(QStringList{"-m", toQString(input)});
  cr.startAndWait();
  if (cr.getFailed()) {
    xfatal(stringb(
      toString(cr.getCommandLine()) << ": " << toString(cr.getErrorMessage())));
  }
  if (cr.getExitCode() != 0) {
    xfatal(stringb(
      toString(cr.getCommandLine()) << ": failed with code " << cr.getExitCode()));
  }
  return trimWhitespace(toString(cr.getOutputLine()));
}


// Normalize a string that represents a directory path prior to
// comparing it to an expected value.
string normalizeDir(string d)
{
  if (SMFileUtil().windowsPathSemantics()) {
    if (beginsWith(d, "/")) {
      // If we want a Windows path but 'd' starts with a slash, then we
      // are probably running on Cygwin, and need to use 'cygpath' to
      // get a Windows path with a drive letter.
      d = runCygpath(d);
    }

    d = replaceAll(d, "\\", "/");
    d = translate(d, "A-Z", "a-z");
    if (d.length() >= 12 &&
        d.substr(0, 10) == "/cygdrive/" &&
        d[11] == '/')
    {
      char letter = d[10];
      d = stringb(letter << ":/" << d.substr(12, d.length()-12));
    }
  }

  // Paths can have whitespace at either end, but rarely do, and I
  // need to discard the newline that 'pwd' prints.
  d = trimWhitespace(d);

  return d;
}


void runCmdDirExpectOutDir(string const &cmd,
  string const &wd, string const &expectDir)
{
  DIAG("run: cmd=" << cmd << " wd=" << wd);
  CommandRunner cr;
  cr.setProgram(toQString(cmd));
  if (!wd.empty()) {
    cr.setWorkingDirectory(toQString(wd));
  }
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);

  string actualDir = cr.getOutputData().constData();

  string expectNormDir = normalizeDir(expectDir);
  string actualNormDir = normalizeDir(actualDir);
  EXPECT_EQ(actualNormDir, expectNormDir);
  DIAG("  as expected, got dir: " << trimWhitespace(actualDir));
}


void runCmdExpectOutDir(string const &cmd, string const &output)
{
  runCmdDirExpectOutDir(cmd, "", output);
}


// ----------------------------- tests ---------------------------------
void testProcessError()
{
  runCmdExpectError("nonexistent-command", QProcess::FailedToStart);
  runCmdArgsExpectError("sleep", QStringList() << "3", QProcess::Timedout);

  // Test that the timeout allows a 1s program to terminate.
  runCmdArgsExpectExit("sleep", QStringList() << "1", 0);
}


void testExitCode()
{
  runCmdExpectExit("true", 0);
  runCmdExpectExit("false", 1);
  runCmdArgsExpectExit("perl", QStringList() << "-e" << "exit(42);", 42);
}


void testOutputData()
{
  runCmdArgsInExpectOut("tr", QStringList() << "a-z" << "A-Z",
    "hello", "HELLO");
  runCmdArgsInExpectOut("tr", QStringList() << "a-z" << "A-Z",
    "one\ntwo\nthree\n", "ONE\nTWO\nTHREE\n");

  runCmdArgsExpectOutErr("sh",
    QStringList() << "-c" << "echo -n to stdout ; echo -n to stderr 1>&2",
    "to stdout", "to stderr");

  runMergedCmdArgsExpectOut("sh",
    QStringList() << "-c" << "echo to stdout ; echo to stderr 1>&2",
    "to stdout\nto stderr\n");

  runMergedCmdArgsExpectOut("sh",
    QStringList() << "-c" << "echo out1 ; echo err1 1>&2; echo out2 ; echo err2 1>&2",
    "out1\nerr1\nout2\nerr2\n");
}


void testStderrFile()
{
  SMFileUtil sfu;

  sfu.createDirectoryAndParents("out");
  std::string errfname = "out/command-runner-test-stderr.txt";

  CommandRunner cr;
  cr.setProgram("sh");
  cr.setArguments(QStringList() <<
    "-c" << "echo -n to stdout ; echo -n to stderr 1>&2");
  cr.setStandardErrorFile(toQString(errfname));
  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData(), "to stdout");
  EXPECT_EQ(cr.getErrorData(), "");
  EXPECT_EQ(sfu.readFileAsString(errfname), "to stderr");
}


void testLargeData1()
{
  DIAG("testing cat on 100kB...");

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

  DIAG("  cat 100kB worked");
}


void testLargeData2(bool swapOrder)
{
  DIAG("testing cat on source code...");

  DataBlock outputDB;
  outputDB.readFromFile("editor-widget.cc");
  QByteArray output((char const *)outputDB.getData(), outputDB.getDataLen());

  DataBlock errorDB;
  errorDB.readFromFile("td-editor-test.cc");
  QByteArray error((char const *)errorDB.getData(), errorDB.getDataLen());

  CommandRunner cr;
  cr.setProgram("sh");

  // In my testing on Windows with cygwin sh, swapping the order of
  // commands in this pipeline does alter the order of events received
  // by my program, so it is good to test both ways.
  cr.setArguments(QStringList() << "-c" <<
    (swapOrder?
      "(cat td-editor-test.cc 1>&2) & cat editor-widget.cc ; wait $!" :
      "cat editor-widget.cc & (cat td-editor-test.cc 1>&2) ; wait $!"));

  cr.startAndWait();
  EXPECT_EQ(cr.getFailed(), false);
  EXPECT_EQ(cr.getOutputData().size(), output.size());
  xassert(cr.getOutputData() == output);
  EXPECT_EQ(cr.getErrorData().size(), error.size());
  xassert(cr.getErrorData() == error);

  DIAG("  cat of source code worked");
}


void testWorkingDirectory()
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
void testMiscDiagnostics()
{
  runCmdArgsIn("cmd", QStringList() << "/c" << "echo %PATH%", "");
  runCmdArgsIn("cmd", QStringList() << "/c" << "set", "");
  runCmdArgsIn("cmd", QStringList() << "/c" << "sort /?", "");

  // If PATH search finds Windows sort, it thinks this input is some
  // multibyte encoding and produces "??????" as output.
  runCmdIn("sort", "a\nc\nb\nz\ny\n1\n");
  runCmdIn("sort", "a\nc\nb\nz\ny\n12\n");
}


void sleepBriefly()
{
  sleepWhilePumpingEvents(200 /*ms*/);
}


// Running a program asynchronously and not using any signals, just
// waiting and polling.
void testAsyncNoSignals()
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


// Like above, but using the "waitFor" methods.
void testAsyncWaitFor()
{
  CommandRunner cr;
  cr.setProgram("cat");
  cr.startAsynchronous();
  EXPECT_EQ(cr.waitForStarted(5000 /*ms*/), true);

  xassert(cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());

  cr.putInputData(QByteArray("hello\n"));
  EXPECT_EQ(cr.waitForOutputLine(), "hello\n");

  cr.putInputData(QByteArray("another\n"));
  EXPECT_EQ(cr.waitForOutputData(8).toStdString(), "another\n");

  cr.closeInputChannel();
  cr.waitForOutputChannelClosed();
  xassert(!cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());
  xassert(!cr.getFailed());
  xassert(cr.getExitCode() == 0);
}


// Like above, but use `waitForQtEvent` instead of the `waitFor`
// methods of `CommandRunner`.
void testAsyncExternalWait()
{
  CommandRunner cr;
  cr.setProgram("cat");
  cr.startAsynchronous();
  EXPECT_EQ(cr.waitForStarted(5000 /*ms*/), true);
  xassert(cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());

  cr.putInputData(QByteArray("hello\n"));
  while (!cr.hasSizedOutputData(6)) {
    waitForQtEvent();
  }
  EXPECT_EQ(cr.takeSizedOutputData(6).toStdString(), "hello\n");

  cr.putInputData(QByteArray("more\n"));
  while (!cr.hasSizedOutputData(5)) {
    waitForQtEvent();
  }
  EXPECT_EQ(cr.takeSizedOutputData(5).toStdString(), "more\n");

  cr.closeInputChannel();
  while (cr.isRunning()) {
    waitForQtEvent();
  }
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());
  xassert(!cr.getFailed());
  xassert(cr.getExitCode() == 0);
}


// Similar, but with a program that writes its output in two steps.
void testAsyncWaitFor_delayedWrite()
{
  CommandRunner cr;
  cr.setProgram("sh");
  cr.setArguments(QStringList() << "-c" <<
    "echo first; sleep 1; echo second");
  cr.startAsynchronous();

  // The point here is the first read should only get 6 bytes, with the
  // remainder coming after one second, but the "waitFor" call should
  // take care of that.
  EXPECT_EQ(cr.waitForOutputData(13).toStdString(),
    "first\nsecond\n");

  cr.waitForNotRunning();
  xassert(cr.getExitCode() == 0);
}


// Now with a program that closes its output but then delays exiting a
// short while.
//
// Note: This test is disabled below, in `main`.  Unfortunately,
// `QProcess` does not properly distinguish between an output stream
// closing and the child process terminating, so my API cannot do so
// either.
//
void testAsyncWaitFor_delayedExit()
{
  CommandRunner cr;
  cr.setProgram("sh");
  cr.setArguments(QStringList() << "-c" <<
    "echo hello; sleep 1; echo there; exec 1>&-; sleep 1");
  cr.startAsynchronous();

  // Channel should be open.
  xassert(cr.outputChannelOpen());

  // The point here is the first read should only get 6 bytes, with the
  // remainder coming after one second, but the "waitFor" call should
  // take care of that.
  EXPECT_EQ(cr.waitForOutputLine(), "hello\n");

  // At this point, the output channel should *not* be closed, even
  // though we have read all of the immediately available data.
  xassert(cr.outputChannelOpen());

  // Wait for and get the next line.
  EXPECT_EQ(cr.waitForOutputLine(), "there\n");

  // My hope is I can see this finish significantly before the next
  // call finishes.
  cr.waitForOutputChannelClosed();
  long channelClosedTime = getMilliseconds();

  // Should see this a little later.
  cr.waitForNotRunning();
  long processClosedTime = getMilliseconds();

  xassert(cr.getExitCode() == 0);

  // This should be around 1000 (1s).
  VPVAL(processClosedTime - channelClosedTime);
}


CLOSE_ANONYMOUS_NAMESPACE


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
{
  // For safety, should disconnect slots in destructor.
  QObject::disconnect(m_commandRunner, NULL, this, NULL);
}


int CRTester::exec()
{
  if (m_commandRunner->isRunning()) {
    return this->QEventLoop::exec();
  }
  else {
    DIAG("CRTester::exec: returning immediately");
    return 0;
  }
}


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
  DIAG("CRTester::slot_processTerminated");

  if (m_protocol == P_CAT || m_protocol == P_ECHO) {
    // Just for extra checking for these two, double-check the status in
    // the signal handler, as well as after exec() returns (which is
    // what all the others do).
    assert(!m_commandRunner->isRunning());
    assert(!m_commandRunner->getFailed());
    assert(m_commandRunner->getExitCode() == 0);
  }

  // Terminate the event loop.
  this->exit(0);
}


OPEN_ANONYMOUS_NAMESPACE


void testAsyncWithSignals()
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


void testAsyncBothOutputs()
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


void testAsyncKill(bool wait)
{
  TEST_CASE("testAsyncKill: wait=" << wait);

  try {
    CommandRunner cr;
    CRTester tester(&cr, wait? CRTester::P_KILL : CRTester::P_KILL_NO_WAIT);

    cr.setProgram("cat");
    cr.startAsynchronous();

    cr.putInputData(QByteArray("hello\n"));

    int res = tester.exec();
    DIAG("testAsyncKill: exec() finished with code " << res);

    if (wait) {
      xassert(!cr.isRunning());
      xassert(!cr.hasOutputData());
      xassert(!cr.hasErrorData());

      xassert(cr.getFailed());
      VPVAL(toString(cr.getErrorMessage()));
      xassert(cr.getProcessError() == QProcess::Crashed);
    }
    else {
      // Since we didn't give the event loop an opportunity to run,
      // QProcess should still think the process is alive.
      xassert(cr.isRunning());

      // TODO: The destructors that run now will print some messages
      // that I would like to normally suppress.
    }
  }
  catch (...) {
    DIAG("testAsyncKill: exception propagating out");
    throw;
  }

  DIAG("testAsyncKill(" << wait << ") finished");
}


void testAsyncFailedStart()
{
  CommandRunner cr;
  CRTester tester(&cr, CRTester::P_FAILED_START);

  cr.setProgram("nonexistent-program");
  cr.startAsynchronous();

  tester.exec();

  xassert(!cr.isRunning());
  xassert(!cr.hasOutputData());
  xassert(!cr.hasErrorData());

  xassert(cr.getFailed());
  VPVAL(toString(cr.getErrorMessage()));
  xassert(cr.getProcessError() == QProcess::FailedToStart);
}


void expectSSCL(char const *input, char const *expect)
{
  CommandRunner r;
  r.setShellCommandLine(input, false /*alwaysUseSH*/);
  string actual = toString(r.getCommandLine());
  EXPECT_EQ(actual, string(expect));
}

// This tests 'setShellCommandLine' when 'alwaysUseSH' is *false*,
// essentially exercising its detection of shell metacharacters.
//
// But, as of 2018-07-16, I'm not using that capability in the editor,
// instead always using 'sh'.
void testSetShellCommandLine()
{
  expectSSCL("date", "date");
  expectSSCL("echo hi", "echo hi");
  expectSSCL("date | date", "sh -c date | date");
  expectSSCL("echo 'hi'", "sh -c echo 'hi'");
}


void printStatus(CommandRunner &runner)
{
  DIAG("CommandRunner running: " << runner.isRunning());
  if (!runner.isRunning()) {
    DIAG("CommandRunner failed: " << runner.getFailed());
    if (runner.getFailed()) {
      DIAG("CommandRunner error: " <<
           toString(runner.getProcessError()));
      DIAG("CommandRunner error message: " <<
           toString(runner.getErrorMessage()));
    }
    else {
      DIAG("CommandRunner exit code: " <<
           runner.getExitCode());
    }
  }
}

// Run a program and then kill it.  This is meant for interactive
// testing.
//
// Adapted from wrk/learn/qt5/qproc.cc.
void runAndKill(CmdlineArgsSpan commandAndArgs)
{
  UnixTime startTime = 0;

  {
    CommandRunner runner;
    runner.setProgram(commandAndArgs[0]);
    QStringList args;
    for (char const *arg : commandAndArgs.subspan(1)) {
      args << arg;
    }
    runner.setArguments(args);

    // Child will inherit stdin/out/err.
    runner.forwardChannels();

    DIAG("starting: " << commandAndArgs[0] << (args.isEmpty()? "" : " ") <<
         args.join(' ').toUtf8().constData());
    runner.startAsynchronous();

    // Wait a moment to reach quiescence.
    DIAG("waiting for 200 ms ...");
    sleepWhilePumpingEvents(200);
    printStatus(runner);

    // Attempt to kill the process.
    DIAG("calling killProcessNoWait ...");
    runner.killProcessNoWait();

    // Wait again.
    DIAG("waiting for 200 ms ...");
    sleepWhilePumpingEvents(200);
    printStatus(runner);

    // Now let the destructor run.
    startTime = getCurrentUnixTime();
    DIAG("destroying CommandRunner ...");
  }

  DIAG("CommandRunner destructor took about " <<
       (getCurrentUnixTime() - startTime) << " seconds");
}


CLOSE_ANONYMOUS_NAMESPACE


#define RUN(statement)                              \
  if (!oneTest || 0==strcmp(oneTest, #statement)) { \
    DIAG("------ " << #statement << " ------");     \
    statement;                                      \
  }


// Called from unit-tests.cc.
void test_command_runner(CmdlineArgsSpan args)
{
  if (!args.empty()) {
    // Special mode for interactive testing of CommandRunner.
    runAndKill(args);
    return;
  }

  // Cygwin is needed for the build anyway, so this should not be a
  // big deal.  I did give some thought to writing the tests so they
  // work work without cygwin, but plain Windows is a very spartan
  // environment.
  if (SMFileUtil().windowsPathSemantics()) {
    // TODO: This message is sort of useless.  I should actually check
    // if the cygwin tools are available.
    DIAG("NOTE: These tests require cygwin on Windows.");
  }

  // Optionally run just one test.
  char const *oneTest = getenv("TEST_CMD_ONE");

  RUN(testProcessError());
  RUN(testExitCode());
  RUN(testOutputData());
  RUN(testStderrFile());
  RUN(testLargeData1());
  RUN(testLargeData2(false));
  RUN(testLargeData2(true));
  RUN(testWorkingDirectory());
  RUN(testAsyncNoSignals());
  RUN(testAsyncWaitFor());
  RUN(testAsyncExternalWait());
  RUN(testAsyncWaitFor_delayedWrite());
  if (false) {
    // Disable the test because it doesn't work the way I would like
    // and costs 2 seconds.
    RUN(testAsyncWaitFor_delayedExit());
  }
  RUN(testAsyncWithSignals());
  RUN(testAsyncBothOutputs());
  RUN(testAsyncKill(true));
  RUN(testAsyncKill(false));
  RUN(testAsyncFailedStart());
  RUN(testMiscDiagnostics());
  RUN(testSetShellCommandLine());
}


// EOF
