// editor-version.cc
// Code for `editor-version` module.

#include "editor-version.h"            // this module

#include "git-version.h"               // editor_git_version
#include "rfa-msg.h"                   // RFA_currentVersion

#include "smbase/stringb.h"            // stringb

#include <string>                      // std::string


std::string getEditorVersionString()
{
  return stringb(
    // `editor_git_version` has a newline.
    "Editor version: " << editor_git_version <<

    "RFA protocol version: " << RFA_currentVersion << "\n");
}


// EOF
