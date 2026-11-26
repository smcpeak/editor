// ui-shell-script-file-test.cc
// Tests for `ui-shell-script-file` module.

// See license.txt for copyright and terms of use.

#include "ui-shell-script-file.h"                // module under test
#include "unit-tests.h"                          // decl for my entry point

// smbase
#include "smbase/exc.h"                          // smbase::XBase
#include "smbase/gdvalue-vector.h"               // gdv::toGDValue(std::vector)
#include "smbase/gdvalue.h"                      // gdv::{GDValue, fromGDVN}
#include "smbase/sm-file-util.h"                 // SMFileUtil
#include "smbase/sm-macros.h"                    // OPEN_ANONYMOUS_NAMESPACE
#include "smbase/sm-test.h"                      // EXPECT_EQ, EXPECT_EXN_REGEX

// libc++
#include <string>                                // std::string
#include <vector>                                // std::vector

using namespace gdv;
using namespace smbase;


OPEN_ANONYMOUS_NAMESPACE


// Scratch directory for the script files.
char const * const scratchDir = "out/ussft";


// Write `contents` to `scratchDir/name` and return that path.
std::string writeScratch(
  std::string const &name,
  std::string const &contents)
{
  SMFileUtil sfu;
  sfu.createDirectoryAndParents(scratchDir);
  std::string path = std::string(scratchDir) + "/" + name;
  sfu.writeFileAsString(path, contents);
  return path;
}


// Write `contents` as a script, read it, and return the result.
UIShellScriptFile readScript(std::string const &contents)
{
  return UIShellScriptFile(writeScratch("script.uis", contents));
}


// The commands of `script`, as GDVN.
std::string commandsString(UIShellScriptFile const &script)
{
  return toGDValue(script.m_commands).asString();
}


void test_defaultCtor()
{
  UIShellScriptFile script;
  EXPECT_EQ(script.m_args.size(), 0);
  EXPECT_EQ(script.m_commands.size(), 0);
}


void test_args()
{
  UIShellScriptFile script = readScript(
    "{ args:[\"a.txt\" \"b.txt\"] cmds:[Sleep(1)] }");
  EXPECT_EQ(toGDValue(script.m_args).asString(),
            "[\"a.txt\" \"b.txt\"]");
  EXPECT_EQ(commandsString(script), "[Sleep(1)]");
}


void test_noArgs()
{
  UIShellScriptFile script = readScript("{ cmds:[Sleep(1)] }");
  EXPECT_EQ(script.m_args.size(), 0);
  EXPECT_EQ(commandsString(script), "[Sleep(1)]");
}


void test_emptyCmds()
{
  UIShellScriptFile script = readScript("{ cmds:[] }");
  EXPECT_EQ(script.m_commands.size(), 0);
}


void test_missingCmds()
{
  EXPECT_EXN_REGEX(readScript("{ args:[] }"), XBase, "cmds");
}


void test_notAMap()
{
  EXPECT_EXN_REGEX(readScript("[Sleep(1)]"), XBase, "map");
}


void test_missingFile()
{
  EXPECT_EXN_REGEX(
    UIShellScriptFile(std::string(scratchDir) + "/nonexistent.uis"),
    XBase, "nonexistent");
}


void test_flatten()
{
  // Nested sequences are flattened, nulls are dropped, and the
  // contents of tuples and tagged sequences are left alone.
  UIShellScriptFile script = readScript(
    "{ cmds:[A(1) [B(2) [C(3) null]] null [] "
    "D([4 [5]]) T[6 [7]]] }");
  EXPECT_EQ(commandsString(script),
            "[A(1) B(2) C(3) D([4 [5]]) T[6 [7]]]");
}


void test_includes()
{
  // The include name is relative to the script's directory.
  writeScratch("defns.gdvn",
    "[ Define(target \"w.x\") "
    "  Define(clickTwice(path) "
    "    [ClickButton(path) ClickButton(path)]) ]");

  UIShellScriptFile script = readScript(
    "{ includes:[\"defns.gdvn\"] "
    "  cmds:[CheckLabel(target \"hi\") clickTwice(\"b\")] }");
  EXPECT_EQ(commandsString(script),
            "[CheckLabel(\"w.x\" \"hi\") "
            "ClickButton(\"b\") ClickButton(\"b\")]");
}


void test_defineInCmds()
{
  // A `Define` in `cmds` itself applies to what follows it.
  UIShellScriptFile script = readScript(
    "{ cmds:[Define(n 5) Sleep(n)] }");
  EXPECT_EQ(commandsString(script), "[Sleep(5)]");
}


void test_missingInclude()
{
  EXPECT_EXN_REGEX(readScript(
    "{ includes:[\"no-such-defns.gdvn\"] cmds:[] }"),
    XBase, "no-such-defns");
}


CLOSE_ANONYMOUS_NAMESPACE


// Called from unit-tests.cc.
void test_ui_shell_script_file(CmdlineArgsSpan args)
{
  test_defaultCtor();
  test_args();
  test_noArgs();
  test_emptyCmds();
  test_missingCmds();
  test_notAMap();
  test_missingFile();
  test_flatten();
  test_includes();
  test_defineInCmds();
  test_missingInclude();
}


// EOF
