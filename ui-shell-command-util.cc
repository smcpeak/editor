// ui-shell-command-util.cc
// Code for `ui-shell-command-util` module.

#include "ui-shell-command-util.h"     // this module

// smbase
#include "smbase/exc.h"                // xmessagesb
#include "smbase/gdvalue.h"            // GDVN_OMAP_EXPRS

// libc++
#include <regex>                       // std::regex_search
#include <string>                      // std::string

using namespace gdv;


// Return true if a substring of 'str' matches 're'.
bool regexSearch(std::string const &str, std::string const &re)
{
  std::regex regex(re.c_str(), std::regex::ECMAScript);
  return std::regex_search(str.c_str(), regex);
}


void checkRegexSearch(
  std::string const &actual, std::string const &expectRE)
{
  if (!regexSearch(actual, expectRE)) {
    xmessagesb("re match fail: " << GDVN_OMAP_EXPRS(actual, expectRE));
  }
}


// EOF
