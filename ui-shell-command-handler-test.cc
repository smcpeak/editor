// ui-shell-command-handler-test.cc
// Tests for `ui-shell-command-handler` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-command-handler-test.h"       // helper classes for this module
#include "ui-shell-command-handler.h"            // module under test
#include "unit-tests.h"                          // decl for my entry point

// editor
#include "file-mod-time.h"                       // getFileModTime
#include "waiting-counter.h"                     // g_waitingCounter

// smbase
#include "smbase/exc.h"                          // smbase::XBase, xmessagesb
#include "smbase/gdvalue.h"                      // gdv::fromGDVN
#include "smbase/map-util.h"                     // smbase::mapFindValuePtrOptC
#include "smbase/nonport.h"                      // getMilliseconds
#include "smbase/run-process.h"                  // RunProcess
#include "smbase/set-util.h"                     // smbase::setContains
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"                      // EXPECT_EQ, EXPECT_EXN_REGEX
#include "smbase/string-util.h"                  // doubleQuote
#include "smbase/stringb.h"                      // stringb

// Qt
#include <QTimer>

// libc++
#include <map>                                   // std::map
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


// --------------------------- FakeQueryable ---------------------------
FakeQueryable::FakeQueryable()
  : QObject(),
    UIShellQueryable(),
    m_values(),
    m_throwingStates(),
    m_delayedState(),
    m_delayedValue()
{}


FakeQueryable::~FakeQueryable()
{}


GDValue FakeQueryable::uiShellQuery(std::string const &state)
{
  if (setContains(m_throwingStates, state)) {
    xmessagesb("query of " << doubleQuote(state) << " failed");
  }

  if (GDValue const *value = mapFindValuePtrOptC(m_values, state)) {
    return *value;
  }

  return UIShellQueryable::uiShellQuery(state);
}


void FakeQueryable::slot_applyDelayedSet() NOEXCEPT
{
  GENERIC_CATCH_BEGIN

  m_values[m_delayedState] = m_delayedValue;

  GENERIC_CATCH_END
}


// ------------------------ TestCommandHandler -------------------------
OPEN_ANONYMOUS_NAMESPACE


// Scratch directory for the file commands.  It must be relative
// because `RecursivelyRemoveFilePath` rejects absolute paths.
char const * const scratchDir = "out/uscht";


// Handler that finds queryables by name in a map.
class TestCommandHandler : public UIShellCommandHandler {
public:      // data
  // Map from path to queryable.
  std::map<std::string, UIShellQueryable *> m_queryables;

public:      // funcs
  TestCommandHandler()
    : UIShellCommandHandler(),
      m_queryables()
  {}

  virtual UIShellQueryable *getQueryableFromPath(
    std::string const &path) OVERRIDE
  {
    if (UIShellQueryable * const * q =
          mapFindValuePtrOptC(m_queryables, path)) {
      return *q;
    }
    else {
      xmessagesb("unknown path: " << doubleQuote(path));
      return nullptr;   // Not reached.
    }
  }

  // Parse `commandGDVN` and execute it.
  GDValue exec(std::string const &commandGDVN)
  {
    return executeCommand(fromGDVN(commandGDVN));
  }
};


// Remove `path` and everything under it, if it exists.
void removeTree(std::string const &path)
{
  // Currently this is only ever called with one argument, and I'm a bit
  // paranoid about a runaway "rm -r".
  xassert(path == scratchDir);

  if (SMFileUtil().pathExists(path)) {
    RunProcess::check_run(std::vector<std::string>{"rm", "-r", path});
  }
}


// Return `scratchDir/name`.
std::string scratch(std::string const &name)
{
  return std::string(scratchDir) + "/" + name;
}


// ------------------------------- tests -------------------------------
void test_unrecognized()
{
  TestCommandHandler handler;

  EXPECT_EXN_REGEX(handler.exec("NoSuchCommand()"), XBase,
    "unrecognized function: \"NoSuchCommand\"");

  // Commands that use `gdvpToTuple`.
  EXPECT_EXN_REGEX(handler.exec("Sleep()"), XBase,
    "funcName=Sleep: .*Expected container to have 1 elements, "
    "but it instead has 0 elements");
  EXPECT_EXN_REGEX(handler.exec("Sleep(1 2)"), XBase,
    "funcName=Sleep: .*Expected container to have 1 elements, "
    "but it instead has 2 elements");

  // Command that uses `BIND_STRING_ARGS1`.
  EXPECT_EXN_REGEX(handler.exec("TouchFile(\"a\" \"b\")"), XBase,
    "incorrect number of arguments to TouchFile; "
    "2 passed but 1 required");

  // Not a tuple.
  EXPECT_EXN_REGEX(handler.exec("3"), XBase,
    "Expected tuple, not small integer");
}


void test_sleep()
{
  TestCommandHandler handler;

  int origCounter = g_waitingCounter;
  long startMS = getMilliseconds();

  EXPECT_EQ(handler.exec("Sleep(50)"), GDValue());

  long elapsedMS = getMilliseconds() - startMS;
  VPVAL(elapsedMS);

  // Qt's default "coarse" timers can fire up to 5% early.
  EXPECT_TRUE(elapsedMS >= 47);

  EXPECT_EQ(g_waitingCounter, origCounter);
}


void test_checkQuery()
{
  FakeQueryable fake;
  fake.m_values["three"] = 3;
  fake.m_values["str"] = "abc";
  fake.m_throwingStates.insert("bad");

  TestCommandHandler handler;
  handler.m_queryables["fake"] = &fake;

  // Match.
  EXPECT_EQ(handler.exec(
    "CheckQuery(\"fake\" \"three\" 3)"), GDValue());
  EXPECT_EQ(handler.exec(
    "CheckQuery(\"fake\" \"str\" \"abc\")"), GDValue());

  // Mismatch.
  EXPECT_EXN_REGEX(handler.exec(
    "CheckQuery(\"fake\" \"three\" 4)"), XBase,
    "CheckQuery \"fake\" \"three\": should have been 4 but was 3\\.");

  // Unknown receiver.
  EXPECT_EXN_REGEX(handler.exec(
    "CheckQuery(\"nope\" \"three\" 3)"), XBase,
    "unknown path: \"nope\"");

  // Unknown state.
  EXPECT_EXN_REGEX(handler.exec(
    "CheckQuery(\"fake\" \"nope\" 3)"), XBase,
    "unknown state: \"nope\"");

  // Query that throws.
  EXPECT_EXN_REGEX(handler.exec(
    "CheckQuery(\"fake\" \"bad\" 3)"), XBase,
    "query of \"bad\" failed");
}


void test_checkQueryMatches()
{
  FakeQueryable fake;
  fake.m_values["three"] = 3;
  fake.m_values["str"] = "hello world";

  TestCommandHandler handler;
  handler.m_queryables["fake"] = &fake;

  // Match.
  EXPECT_EQ(handler.exec(
    "CheckQueryMatches(\"fake\" \"str\" \"lo w\")"), GDValue());
  EXPECT_EQ(handler.exec(
    "CheckQueryMatches(\"fake\" \"str\" \"^h.*d$\")"), GDValue());

  // Mismatch.
  EXPECT_EXN_REGEX(handler.exec(
    "CheckQueryMatches(\"fake\" \"str\" \"^world\")"), XBase,
    "CheckQueryMatches \"fake\" \"str\": the actual string "
    "\"hello world\" did not match the regex \"\\^world\"\\.");

  // Actual value is not a string.
  EXPECT_EXN_REGEX(handler.exec(
    "CheckQueryMatches(\"fake\" \"three\" \"3\")"), XBase,
    "the actual value 3 is not a string");
}


void test_waitUntilCheckQuery()
{
  FakeQueryable fake;
  fake.m_values["v"] = 1;

  TestCommandHandler handler;
  handler.m_queryables["fake"] = &fake;

  // Already true.
  EXPECT_EQ(handler.exec(
    "WaitUntilCheckQuery(1000 \"fake\" \"v\" 1)"), GDValue());

  // Becomes true after a timer fires.
  //
  // Ideally the wait would end right after the slot runs, but on
  // Windows with `QCoreApplication`, the `processEvents` call that runs
  // the slot keeps waiting until the next event, which here is the end
  // of the wait period.  The final check then succeeds.  So, this uses
  // a short period and does not check the elapsed time.
  //
  // TODO: I think this can be fixed.
  {
    fake.m_delayedState = "v";
    fake.m_delayedValue = 2;
    QTimer::singleShot(50 /*ms*/,
      &fake, &FakeQueryable::slot_applyDelayedSet);

    long startMS = getMilliseconds();
    EXPECT_EQ(handler.exec(
      "WaitUntilCheckQuery(500 \"fake\" \"v\" 2)"), GDValue());
    VPVAL(getMilliseconds() - startMS);
  }

  // Times out.
  EXPECT_EXN_REGEX(handler.exec(
    "WaitUntilCheckQuery(100 \"fake\" \"v\" 3)"), XBase,
    "WaitUntilCheckQuery: Slept for [0-9]+ ms but value is 2, not 3");
}


void test_fileContents()
{
  TestCommandHandler handler;
  std::string fname = scratch("contents.txt");

  EXPECT_EQ(handler.exec(stringb(
    "WriteFileContents(" << doubleQuote(fname) <<
    " \"one\\ntwo\\n\")")),
    GDValue());
  EXPECT_EQ(SMFileUtil().readFileAsString(fname), "one\ntwo\n");

  // Match.
  EXPECT_EQ(handler.exec(stringb(
    "CheckFileContents(" << doubleQuote(fname) <<
    " \"one\\ntwo\\n\")")),
    GDValue());

  // Mismatch.
  EXPECT_EXN_REGEX(handler.exec(stringb(
    "CheckFileContents(" << doubleQuote(fname) << " \"one\\n\")")),
    XBase,
    "should have been \"one\\\\n\" but was \"one\\\\ntwo\\\\n\"");
}


void test_checkPathExists()
{
  TestCommandHandler handler;
  std::string fname = scratch("exists.txt");
  SMFileUtil().writeFileAsString(fname, "x");

  EXPECT_EQ(handler.exec(stringb(
    "CheckPathExists(" << doubleQuote(fname) << " true)")), GDValue());
  EXPECT_EQ(handler.exec(stringb(
    "CheckPathExists(" << doubleQuote(scratch("nope")) << " false)")),
    GDValue());

  EXPECT_EXN_REGEX(handler.exec(stringb(
    "CheckPathExists(" << doubleQuote(fname) << " false)")), XBase,
    "should have been false but was true");
  EXPECT_EXN_REGEX(handler.exec(stringb(
    "CheckPathExists(" << doubleQuote(scratch("nope")) << " true)")),
    XBase,
    "should have been true but was false");
}


void test_removeFileIfExists()
{
  TestCommandHandler handler;
  std::string fname = scratch("remove.txt");
  SMFileUtil().writeFileAsString(fname, "x");

  // Present.
  EXPECT_EQ(handler.exec(stringb(
    "RemoveFileIfExists(" << doubleQuote(fname) << ")")), GDValue());
  EXPECT_FALSE(SMFileUtil().pathExists(fname));

  // Absent.
  EXPECT_EQ(handler.exec(stringb(
    "RemoveFileIfExists(" << doubleQuote(fname) << ")")), GDValue());
  EXPECT_FALSE(SMFileUtil().pathExists(fname));
}


void test_recursivelyRemoveFilePath()
{
  TestCommandHandler handler;
  std::string dir = scratch("tree");
  SMFileUtil().createDirectoryAndParents(dir + "/sub");
  SMFileUtil().writeFileAsString(dir + "/a.txt", "a");
  SMFileUtil().writeFileAsString(dir + "/sub/b.txt", "b");

  // Directory with contents.
  EXPECT_EQ(handler.exec(stringb(
    "RecursivelyRemoveFilePath(" << doubleQuote(dir) << ")")),
    GDValue());
  EXPECT_FALSE(SMFileUtil().pathExists(dir));

  // Nonexistent path.
  EXPECT_EQ(handler.exec(stringb(
    "RecursivelyRemoveFilePath(" << doubleQuote(dir) << ")")),
    GDValue());

  // Rejected paths.
  EXPECT_EXN(handler.exec(
    "RecursivelyRemoveFilePath(\"/tmp/uscht-nonexistent\")"), XBase);
  EXPECT_EXN(handler.exec(
    "RecursivelyRemoveFilePath(\"out/uscht/../uscht\")"), XBase);
  EXPECT_TRUE(SMFileUtil().pathExists(scratchDir));
}


void test_touchFile()
{
  TestCommandHandler handler;
  std::string fname = scratch("touch.txt");

  // Creates a missing file.
  EXPECT_FALSE(SMFileUtil().pathExists(fname));
  EXPECT_EQ(handler.exec(stringb(
    "TouchFile(" << doubleQuote(fname) << ")")), GDValue());
  EXPECT_TRUE(SMFileUtil().pathExists(fname));

  // Changes the modification time of an existing file.  Since the file
  // was just created, this usually requires `TouchFile` to sleep for
  // one second and retry.
  FileTimestamp before = getFileModTime(fname);
  EXPECT_EQ(handler.exec(stringb(
    "TouchFile(" << doubleQuote(fname) << ")")), GDValue());
  FileTimestamp after = getFileModTime(fname);
  VPVAL(before);
  VPVAL(after);
  EXPECT_TRUE(after > before);
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_ui_shell_command_handler(CmdlineArgsSpan args)
{
  test_unrecognized();
  test_sleep();
  test_checkQuery();
  test_checkQueryMatches();
  test_waitUntilCheckQuery();

  removeTree(scratchDir);
  SMFileUtil().createDirectoryAndParents(scratchDir);

  test_fileContents();
  test_checkPathExists();
  test_removeFileIfExists();
  test_recursivelyRemoveFilePath();
  test_touchFile();

  removeTree(scratchDir);
}


// EOF
