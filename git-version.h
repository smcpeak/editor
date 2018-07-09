// git-version.h
// Declare 'editor_git_version'.

#ifndef GIT_VERSION_H
#define GIT_VERSION_H

// A string like "d36671f 2018-07-09 04:10:32 -0700\n" that describes
// the current version in git.  It has a newline and then a NUL
// terminator.
extern char const editor_git_version[];

#endif // GIT_VERSION_H
