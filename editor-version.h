// editor-version.h
// Version-related functions for the editor as a whole.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_EDITOR_VERSION_H
#define EDITOR_EDITOR_VERSION_H

#include "smbase/std-string-fwd.h"     // std::string


// Return a string suitable for the output of the "-version" option.
// This string ends with a newline.
std::string getEditorVersionString();


#endif // EDITOR_EDITOR_VERSION_H
