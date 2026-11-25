// ui-shell-command-util.h
// Macros and functions shared by the UI shell command handlers.

// See license.txt for copyright and terms of use.

// This file defines macros with generic names, so it should only be
// included by .cc files that implement UI shell commands.

#ifndef EDITOR_UI_SHELL_COMMAND_UTIL_H
#define EDITOR_UI_SHELL_COMMAND_UTIL_H

// smbase
#include "smbase/exc.h"                // xmessagesb
#include "smbase/gdvalue.h"            // GDVN_OMAP_EXPRS
#include "smbase/sm-is-equal.h"        // smbase::is_equal
#include "smbase/string-util.h"        // doubleQuote

// libc++
#include <string>                      // std::string


// Return true if a substring of 'str' matches 're'.
bool regexSearch(std::string const &str, std::string const &re);


// Throw unless a substring of `actual` matches `expectRE`.
void checkRegexSearch(
  std::string const &actual, std::string const &expectRE);


template <typename AT, typename ET>
void checkEquality(
  char const *actualName,
  AT const &actual,
  ET const &expect)
{
  // Needed by `GDVN_OMAP_EXPRS`.
  using gdv::toGDValue;

  if (!smbase::is_equal(actual, expect)) {
    xmessagesb(actualName << ": mismatch: " <<
               GDVN_OMAP_EXPRS(actual, expect));
  }
}

#define CHECK_EQUALITY(actual, expect) \
  checkEquality(#actual, actual, expect)


// The following macros are meant to be used inside a function that
// executes UI shell commands, and refer to these local variables:
//
//   funcName: Name of the command (its tag).
//   numArgs:  Number of arguments it has.
//   parser:   `GDValueParser` for the command.
//
// Some of them also refer to `actual`, `expect`, and `expectRE`.

// Complain unless 'numArgs==required'.
#define CHECK_NUM_ARGS(required)                           \
  if (numArgs != required) {                               \
    xmessagesb("incorrect number of arguments to " <<      \
               funcName << "; " << numArgs <<              \
               " passed but " << required << " required"); \
  }                                                        \
  else ((void)0) /* user ; */


// Return 0-based argument `n` as a string.
#define GET_STRING_ARG(n) \
  parser.tupleGetValueAt(n).stringGet()


// Bind 'arg1' to the single expected string argument of a UI shell
// command.
#define BIND_STRING_ARGS1(arg1)                     \
  CHECK_NUM_ARGS(1);                                \
  std::string arg1 = GET_STRING_ARG(0) /* user ; */

#define BIND_STRING_ARGS2(arg1, arg2)               \
  CHECK_NUM_ARGS(2);                                \
  std::string arg1 = GET_STRING_ARG(0);             \
  std::string arg2 = GET_STRING_ARG(1) /* user ; */

#define BIND_STRING_ARGS3(arg1, arg2, arg3)         \
  CHECK_NUM_ARGS(3);                                \
  std::string arg1 = GET_STRING_ARG(0);             \
  std::string arg2 = GET_STRING_ARG(1);             \
  std::string arg3 = GET_STRING_ARG(2) /* user ; */

#define BIND_STRING_ARGS4(arg1, arg2, arg3, arg4)   \
  CHECK_NUM_ARGS(4);                                \
  std::string arg1 = GET_STRING_ARG(0);             \
  std::string arg2 = GET_STRING_ARG(1);             \
  std::string arg3 = GET_STRING_ARG(2);             \
  std::string arg4 = GET_STRING_ARG(3) /* user ; */


#define CHECK_EQ(context)                          \
  if (actual != expect) {                          \
    xmessagesb(context << ": should have been " << \
      toGDValue(expect) << " but was " <<          \
      toGDValue(actual) << ".");                   \
  }

#define CHECK_RE_MATCH(context)                             \
  if (!regexSearch(actual, expectRE)) {                     \
    xmessagesb(context << ": the actual string " <<         \
      doubleQuote(actual) << " did not match the regex " << \
      doubleQuote(expectRE) << ".");                        \
  }


#endif // EDITOR_UI_SHELL_COMMAND_UTIL_H
