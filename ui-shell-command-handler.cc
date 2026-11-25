// ui-shell-command-handler.cc
// Code for `ui-shell-command-handler` module.

#include "ui-shell-command-handler.h"  // this module

// NOTE: There should not be any dependencies here on the editor per se.
// If you want to, say, query an EditorWidget from a test script, use
// the "CheckQuery" generic query.

// editor
#include "file-mod-time.h"             // getFileModTime
#include "ui-shell-command-util.h"     // BIND_STRING_ARGS1, etc.
#include "ui-shell-queryable.h"        // UIShellQueryable
#include "waiting-counter.h"           // IncDecWaitingCounter

// smqtutil
#include "smqtutil/timer-event-loop.h" // sleepWhilePumpingEvents

// smbase
#include "smbase/exc.h"                // EXN_CONTEXT, xmessagesb
#include "smbase/gdvalue-parser.h"     // gdv::GDValueParser
#include "smbase/gdvalue-tuple.h"      // gdv::gdvpToTuple
#include "smbase/gdvalue.h"            // gdv::GDValue
#include "smbase/nonport.h"            // getMilliseconds
#include "smbase/overflow.h"           // safeToInt
#include "smbase/run-process.h"        // RunProcess
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/sm-trace.h"           // INIT_TRACE, etc.
#include "smbase/string-util.h"        // doubleQuote, hasSubstring
#include "smbase/xassert.h"            // xassert, xfailurePrecondition

// Qt
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

// libc++
#include <functional>                  // std::function
#include <string>                      // std::string
#include <vector>                      // std::vector

using namespace gdv;
using namespace smbase;


INIT_TRACE("ui-shell-command-handler");


UIShellCommandHandler::UIShellCommandHandler()
  : SerfRefCount()
{}


UIShellCommandHandler::~UIShellCommandHandler()
{}


GDValue UIShellCommandHandler::executeDerivedCommand(
  std::string const &funcName,
  GDValueParser const &parser)
{
  xmessagesb("unrecognized function: " << doubleQuote(funcName));
  return GDValue();    // not reached
}


GDValue UIShellCommandHandler::executeCommand(GDValue const &command)
{
  GDValueParser parser(command);
  parser.checkIsTuple();

  std::string const funcName(parser.taggedContainerGetTagName());
  EXN_CONTEXT_EXPR(funcName);

  int const numArgs = safeToInt(parser.containerSize());

  // ----------------------------- actions -----------------------------
  if (funcName == "Sleep") {
    auto [duration] =
      gdvpToTuple<int>(parser);

    sleepForMS(duration);
  }

  // ----------------------------- checks ------------------------------
  else if (funcName == "WaitUntilCheckQuery") {
    auto [durationMS, receiver, state, expect] =
      gdvpToTuple<int, std::string, std::string, GDValue>(parser);

    this->waitUntilCheckQuery(
      durationMS, receiver, state, expect);
  }

  else if (funcName == "CheckQuery") {
    auto [receiver, state, expect] =
      gdvpToTuple<std::string, std::string, GDValue>(parser);

    // Provide the location of the expect string in case the
    // `CheckQuery` came from an included file.
    EXN_CONTEXT(expect.sourceLocation().asString());

    UIShellQueryable *q = this->getQueryableFromPath(receiver);
    GDValue actual = q->uiShellQuery(state);
    CHECK_EQ("CheckQuery " << doubleQuote(receiver) << ' ' <<
             doubleQuote(state));
  }

  else if (funcName == "CheckQueryMatches") {
    auto [receiver, state, expectRE] =
      gdvpToTuple<std::string, std::string, std::string>(parser);

    UIShellQueryable *q = this->getQueryableFromPath(receiver);
    GDValue actualValue = q->uiShellQuery(state);
    if (!actualValue.isString()) {
      xmessagesb("CheckQueryMatches: the actual value " <<
        actualValue << " is not a string; expected it to the regex " <<
        doubleQuote(expectRE) << ".");
    }
    std::string actual = actualValue.stringGet();
    CHECK_RE_MATCH("CheckQueryMatches " << doubleQuote(receiver) << ' ' <<
                   doubleQuote(state));
  }

  // ------------------------- file operations -------------------------
  else if (funcName == "TouchFile") {
    BIND_STRING_ARGS1(fname);

    FileTimestamp beforeModUnixTime = getFileModTime(fname);

    SMFileUtil sfu;
    sfu.touchFile(fname);

    FileTimestamp afterModUnixTime = getFileModTime(fname);

    if (beforeModUnixTime == afterModUnixTime &&
        beforeModUnixTime != 0) {
      // The purpose of `TouchFile` is to get a file with a different
      // timestamp, but depending on which tests run in what order, it
      // could be that the file was modified so recently that this
      // "touch" did not affect it at the granularity we measure (one
      // second).  Therefore, sleep one second and try again.
      TRACE1("TouchFile: unchanged file modification time, sleeping...");
      sleepForMS(1000);

      sfu.touchFile(fname);

      // There's not much point in checking again.  If it worked, great.
      // If not, I don't want to sit in a loop, so just keep going.
    }
  }

  else if (funcName == "RemoveFileIfExists") {
    auto [fname] =
      gdvpToTuple<std::string>(parser);

    SMFileUtil().removeFileIfExists(fname);
  }

  else if (funcName == "RecursivelyRemoveFilePath") {
    auto [path] =
      gdvpToTuple<std::string>(parser);

    // For safety, ensure the path is not absolute and has no "..".
    xassert(!SMFileUtil().isAbsolutePath(path));
    xassert(!hasSubstring(path, ".."));

    if (!SMFileUtil().pathExists(path)) {
      // Nothing to do.
    }
    else {
      // I should not need "-f" here, in part because I have confirmed
      // that the path exists.
      RunProcess::check_run(std::vector<std::string>{"rm", "-r", path});
    }
  }

  else if (funcName == "CheckPathExists") {
    auto [path, expect] =
      gdvpToTuple<std::string, bool>(parser);

    bool actual = SMFileUtil().pathExists(path);
    CHECK_EQ("CheckPathExists " << doubleQuote(path));
  }

  else if (funcName == "CheckFileContents") {
    auto [fname, expect] =
      gdvpToTuple<std::string, std::string>(parser);

    std::string actual = SMFileUtil().readFileAsString(fname);
    CHECK_EQ("CheckFileContentsSize " << doubleQuote(fname));
  }

  else if (funcName == "WriteFileContents") {
    auto [fname, contents] =
      gdvpToTuple<std::string, std::string>(parser);

    SMFileUtil().writeFileAsString(fname, contents);
  }

  else {
    return this->executeDerivedCommand(funcName, parser);
  }

  // None of the commands implemented here produce a value.
  return GDValue();
}


bool UIShellCommandHandler::hasPendingStep() const
{
  return false;
}


void UIShellCommandHandler::executePendingStep()
{
  xfailurePrecondition("no pending steps");
}


void UIShellCommandHandler::discardPendingSteps()
{}


void UIShellCommandHandler::sleepForMS(int ms)
{
  TRACE1("sleeping for " << ms << " ms");
  IncDecWaitingCounter idwc;
  sleepWhilePumpingEvents(ms);
  TRACE1("done sleeping");
}


void UIShellCommandHandler::waitUntilCheckQuery(
  long durationMS,
  std::string const &receiver,
  std::string const &state,
  GDValue const &expect)
{
  auto gdvFunc = [this, receiver, state]() -> GDValue {
    UIShellQueryable *q = this->getQueryableFromPath(receiver);
    return q->uiShellQuery(state);
  };
  waitUntilCheckGDValueFunction(durationMS, gdvFunc, expect);
}


void UIShellCommandHandler::waitUntilCheckGDValueFunction(
  long durationMS,
  std::function<GDValue ()> gdvFunc,
  GDValue const &expect)
{
  long startMS = getMilliseconds();
  int checkCount = 0;

  TRACE1("waiting for up to " << durationMS << " ms");

  // Put the `expect` location onto the context stack in case we are
  // executing a function from another file.
  EXN_CONTEXT(expect.sourceLocation().asString());

  // Arrange to receive an event after 'durationMS'.  We do not directly
  // handle the event; rather, we use it to cause 'processEvents' to
  // return.
  QTimer timer;
  timer.start(durationMS);

  while (true) {
    checkCount++;
    GDValue actual = gdvFunc();
    if (actual == expect) {
      break;
    }
    long elapsedMS = getMilliseconds() - startMS;
    long remainingMS = durationMS - elapsedMS;
    if (remainingMS <= 0) {
      xmessagesb("WaitUntilCheckQuery: Slept for " << elapsedMS <<
                 " ms but value is " << actual << ", not " <<
                 expect);
    }

    // Wait for something to happen.  This does not busy-wait.
    IncDecWaitingCounter idwc;
    QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);

    // I tried using the 'processEvents' that accepts a timeout, but
    // that one appears to ignore the 'WaitForMoreEvents' flag.
  }

  long elapsedMS = getMilliseconds() - startMS;
  TRACE1("condition satisfied after " << elapsedMS <<
         " ms and " << checkCount << " checks");
}


// EOF
