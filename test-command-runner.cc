// test-command-runner.cc
// Tests for 'command-runner' module.

#include "command-runner.h"            // module to test

// smqtutil
#include "qtutil.h"                    // toString

// smbase
#include "datablok.h"                  // DataBlock
#include "sm-file-util.h"              // SMFileUtil
#include "strutil.h"                   // replace
#include "test.h"                      // ARGS_MAIN
#include "trace.h"                     // TRACE_ARGS, EXPECT_EQ

// Qt
#include <QCoreApplication>
#include <QProcessEnvironment>


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
      "(cat test-td-editor.cc 1>&2) & cat editor-widget.cc" :
      "cat editor-widget.cc & (cat test-td-editor.cc 1>&2)"));

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


static void entry(int argc, char **argv)
{
  TRACE_ARGS();

  QCoreApplication app(argc, argv);

  if (false) {
    QProcessEnvironment env(QProcessEnvironment::systemEnvironment());
    QString path = env.value("PATH");
    cout << "PATH: " << toString(path) << endl;
  }

  // Cygwin is needed for the build anyway, so this should not be a
  // big deal.  I did give some thought to writing the tests so they
  // work work without cygwin, but plain Windows is a very spartan
  // environment.
  cout << "NOTE: These tests require cygwin on Windows.\n";

  testProcessError();
  testExitCode();
  testOutputData();
  testLargeData1();
  testLargeData2(false);
  testLargeData2(true);
  testWorkingDirectory();

  testMiscDiagnostics();

  cout << "test-command-runner tests passed" << endl;
}

ARGS_TEST_MAIN


// EOF
