// editor-navigation-options.h
// `EditorNavigationOptions`, to control how certain navigation commands
// operate.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_EDITOR_NAVIGATION_OPTIONS_H
#define EDITOR_EDITOR_NAVIGATION_OPTIONS_H

// Controls how certain navigation commands operate.
enum class EditorNavigationOptions {
  // Defaults.
  ENO_NORMAL,

  // Navigate to the specified destination in a different window from
  // the one where the request was made.
  ENO_OTHER_WINDOW,

  NUM_NAVIGATION_OPTIONS
};

// Return a string like "ENO_NORMAL".
char const *toString(EditorNavigationOptions op);

#endif // EDITOR_EDITOR_NAVIGATION_OPTIONS_H
