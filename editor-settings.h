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
};


#endif // EDITOR_SETTINGS_H
