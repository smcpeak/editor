// ui-shell-script-file.h
// `UIShellScriptFile`, the contents of a .uis file.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UI_SHELL_SCRIPT_FILE_H
#define EDITOR_UI_SHELL_SCRIPT_FILE_H

#include "ui-shell-script-file-fwd.h"            // fwds for this module

// smbase
#include "smbase/gdvalue.h"                      // gdv::GDValue

// libc++
#include <string>                                // std::string
#include <vector>                                // std::vector


// The contents of a .uis file, after substitution and flattening.
class UIShellScriptFile {
public:      // data
  // The `args` entry, or empty if it is absent.  Its meaning is up to
  // the program; the editor opens them as files.
  std::vector<std::string> m_args;

  // The `cmds` entry, with the `Define`s from `includes` and `cmds`
  // substituted, and nested sequences and nulls flattened away.
  std::vector<gdv::GDValue> m_commands;

public:      // funcs
  UIShellScriptFile();

  // Read and process `fname`.  Names in `includes` are relative to the
  // directory containing `fname`.  Throws on I/O, syntax and structure
  // errors.
  explicit UIShellScriptFile(std::string const &fname);
};


#endif // EDITOR_UI_SHELL_SCRIPT_FILE_H
