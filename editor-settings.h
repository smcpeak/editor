// editor-settings.h
// `EditorSettings` class

#ifndef EDITOR_SETTINGS_H
#define EDITOR_SETTINGS_H

#include "editor-settings-fwd.h"                 // fwds for this module

#include "editor-command.ast.gen.fwd.h"          // EditorCommand

#include "smbase/gdvalue-fwd.h"                  // gdv::GDValue

#include <map>                                   // std::map
#include <memory>                                // std::unique_ptr
#include <set>                                   // std::set
#include <string>                                // std::string
#include <vector>                                // std::vector


// A vector of commands, functioning as the definition of a macro.
typedef std::vector<std::unique_ptr<EditorCommand>> EditorCommandVector;

// A map from macro name to definition.
typedef std::map<std::string, EditorCommandVector> MacroDefinitionMap;

// Set of command lines.  None of the strings is empty.
typedef std::set<std::string> CommandLineSet;


// History of command lines for use in some particular context, such as
// the Alt+X "Apply Command" function.
class CommandLineHistory final {
public:      // data
  // Set of all command in the history.  Nominally this is all commands
  // ever executed, but the user can delete any ones they do not want to
  // keep.
  CommandLineSet m_commands;

  // Most recently used command.  Normally this should be a string in
  // `m_commands`, but it could be empty to indicate there is no recent
  // command, or it might be one that has since been deleted.
  std::string m_recent;

  // When a command line is executed, it can optionally have elements
  // like "$f" replaced with the document's file name.  This records the
  // current state of that option.  Whenever the user runs a command, it
  // is updated to reflect the choice for that run.
  bool m_useSubstitution;

public:      // funcs
  ~CommandLineHistory();

  // Empty history, no recent command, substitution=true.
  CommandLineHistory();

  // De/serialization.
  operator gdv::GDValue() const;
  explicit CommandLineHistory(gdv::GDValue const &v);

  void swap(CommandLineHistory &obj);

  // Add `cmd` to `m_commands`, set `m_recent` to it, and set
  // `m_useSubstitution`.  Return true if anything changed.
  bool add(std::string const &cmd, bool useSubstitution);

  // Delete `cmd` from `m_applyCommands`.  Clear `m_recent` if it equals
  // `cmd`.  Return true if a change was made.
  bool remove(std::string const &cmd);
};


// Editor-wide persistent user settings.
class EditorSettings {
private:     // data
  // Map from macro name to a sequence of commands to execute.  Every
  // macro has a non-empty name and a non-empty command vector.
  MacroDefinitionMap m_macros;

  // Name of the most recently run macro.  The expectation is this is
  // the name of something in `m_macros`, but desync might be possible.
  // This could be the empty string, meaning no recent macro is
  // recorded.
  std::string m_mostRecentlyRunMacro;

  // History of commands associated with Alt+A "Apply Command".
  CommandLineHistory m_applyHistory;

public:      // funcs
  ~EditorSettings();

  // Initialize with defaults.
  EditorSettings();

  // Serialize as GDV.
  operator gdv::GDValue() const;

  // Parse `gdv`.  Throws `XFormat` on error.
  explicit EditorSettings(gdv::GDValue const &gdv);

  void swap(EditorSettings &obj);

  // ----------------------------- macros ------------------------------
  // Add a macro to `m_settings.m_macros`, replacing any existing one
  // with the same name.  Requires that `name` not be empty and
  // `command` not be empty.
  void addMacro(std::string const &name,
                EditorCommandVector const &commands);

  // Delete the macro called `name` if one exists.  Return true if one
  // was, deleted false otherwise.
  bool deleteMacro(std::string const &name);

  // Return the set of defined macro names.
  std::set<std::string> getMacroNames() const;

  // Return the sequence of commands for `name`, or an empty sequence if
  // it is not defined.
  EditorCommandVector getMacro(std::string const &name) const;

  // Set `m_mostRecentlyRunMacro`.
  void setMostRecentlyRunMacro(std::string const &name);

  // Get `m_mostRecentlyRunMacro`, except if that is not a valid key
  // in `m_macros`, clear it first.  Returns "" if there is no recently
  // run macro.
  std::string getMostRecentlyRunMacro();

  // Just get the current value without validation.
  std::string getMostRecentlyRunMacroC() const
    { return m_mostRecentlyRunMacro; }

  // ---------------------------- commands -----------------------------
  CommandLineHistory const &getApplyHistory() const
    { return m_applyHistory; }

  // Add `cmd` to the set and make it the most recent, and set the
  // substitution flag.  Return true if something changed.
  bool addApplyCommand(std::string const &cmd, bool useSubstitution);

  // Remove `cmd` from `m_applyCommands`.  Return false iff it was not
  // there to begin with.
  bool removeApplyCommand(std::string const &cmd);
};


#endif // EDITOR_SETTINGS_H
