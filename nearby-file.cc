// nearby-file.cc
// code for nearby-file.h

#include "nearby-file.h"               // this module

#include "sm-file-util.h"              // SMFileUtil



// Return true if 'c' is a character that commonly appears in the name
// of text files.
static bool isFilenameChar(char c)
{
  return (('A' <= c && c <= 'Z') ||
          ('a' <= c && c <= 'z') ||
          ('0' <= c && c <= '9') ||
          c == '_' ||
          c == '-' ||
          c == '/' ||
          c == '\\' ||       // for Windows
          c == '.');
}


// Search at 'charOffset' in 'haystack' for candidate file names.
//
// Right now this returns at most one string, but I am designing the
// interface to anticipate the ability to return multiple candidates.
// For example, a first candidate might never consider filenames to
// have spaces, but the next candidate might allow spaces, etc.
static void getCandidateSuffixes(
  ArrayStack<string> /*OUT*/ &candidates,
  string const &haystack,
  int charOffset)
{
  candidates.clear();

  // I want this to be usable without too much pre-checking, so just
  // silently discard an invalid offset or empty string.
  if (haystack.isempty()) {
    return;
  }
  int haystackLength = haystack.length();
  if (!( 0 <= charOffset && charOffset <= haystackLength )) {
    return;
  }

  // Treat the EOL location as referring to the last character.  That
  // way I can just hit End to go to EOL and then Ctrl+I to open the
  // file at the end of that line.
  if (charOffset == haystackLength) {
    charOffset = haystackLength-1;
  }

  // Should be on or just beyond a filename character to begin with.
  if (!isFilenameChar(haystack[charOffset])) {
    if (charOffset > 0 && isFilenameChar(haystack[charOffset-1])) {
      charOffset--;
    }
    else {
      return;
    }
  }

  // Expand the range to include as many valid chars as possible.
  int low = charOffset;
  while (low > 0 && isFilenameChar(haystack[low-1])) {
    low--;
  }
  int high = charOffset;
  while (high < haystackLength-1 && isFilenameChar(haystack[high+1])) {
    high++;
  }

  // Return what we found.
  candidates.push(haystack.substring(low, high-low+1));
}


string getNearbyFilename(
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset)
{
  SMFileUtil sfu;
  return innerGetNearbyFilename(sfu, candidatePrefixes,
    haystack, charOffset);
}


string innerGetNearbyFilename(
  SMFileUtil &sfu,
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset)
{
  if (candidatePrefixes.isEmpty()) {
    return "";
  }

  // Extract candidate suffixes.
  ArrayStack<string> candidateSuffixes;
  getCandidateSuffixes(candidateSuffixes, haystack, charOffset);
  if (candidateSuffixes.isEmpty()) {
    return "";
  }

  // Look for a combination that exists on disk.
  for (int prefix=0; prefix < candidatePrefixes.length(); prefix++) {
    for (int suffix=0; suffix < candidateSuffixes.length(); suffix++) {
      string candidate = sfu.joinFilename(
        candidatePrefixes[prefix], candidateSuffixes[suffix]);
      if (sfu.absolutePathExists(candidate)) {
        return candidate;
      }
    }
  }

  // No combination exists.  Return the first prefix+suffix.  The user
  // will get an opportunity to edit or cancel.
  return sfu.joinFilename(candidatePrefixes[0], candidateSuffixes[0]);
}


// EOF
