// nearby-file.cc
// code for nearby-file.h

#include "nearby-file.h"               // this module

// editor
#include "codepoint.h"                 // isDecimalDigit, isLetter, etc.

// smbase
#include "sm-file-util.h"              // SMFileUtil


// Return true if 'c' is a digit for the purpose of the file name
// recognition algorithm.
static bool isFilenameDigit(char c)
{
  return isDecimalDigit(c);
}

// Return true if 'c' is a character that makes up the "core" of a text
// file name, in the sense that the majority of characters are "core"
// characters.
static bool isFilenameCore(char c)
{
  return isLetter(c) || isFilenameDigit(c);
}

// True if 'c' is punctuation that commonly appears in text file names,
// including as a directory separator.
static bool isFilenamePunctuation(char c)
{
  return c == '_' ||
         c == '-' ||
         c == '/' ||
         c == '\\' ||       // for Windows
         c == '.';
}

// Return true if 'c' is a character that commonly appears in the name
// of text files.
static bool isFilenameChar(char c)
{
  return isFilenameCore(c) || isFilenamePunctuation(c);
}


// If the characters at 'haystack[i]' look like a line number in the
// form ":$N", return the number, otherwise return 0.
static int getLineNumberAt(string const &haystack, int i)
{
  char const *start = haystack.c_str() + i;
  char const *p = start;
  if (*p == ':') {
    p++;
    int ret = 0;
    while (isFilenameDigit(*p) && (p-start < 10)) {
      ret = ret*10 + (*p - '0');
      p++;
    }

    // We should have ended on something other than a digit or letter.
    if (isFilenameCore(*p)) {
      return 0;
    }

    return ret;
  }

  return 0;
}


// Search at 'charOffset' in 'haystack' for candidate file names.
//
// Right now this returns at most one candidate, but I am designing the
// interface to anticipate the ability to return multiple candidates.
// For example, a first candidate might never consider filenames to
// have spaces, but the next candidate might allow spaces, etc.
void getCandidateSuffixes(
  ArrayStack<FileAndLineOpt> /*OUT*/ &candidates,
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

  // Should not start on a digit.  For example, if the cursor is on the
  // "3" in "foo:3", I do not want to treat "3" as the file name.
  if (isFilenameDigit(haystack[charOffset])) {
    return;
  }

  // File names do not usually end with punctuation, and the cursor
  // should not be on consecutive punctuation if it really is on a
  // file name.
  if (isFilenamePunctuation(haystack[charOffset]) &&
      (charOffset == haystackLength-1 ||
       !isFilenameCore(haystack[charOffset+1]))) {
    return;
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

  // Remove trailing punctuation beyond the original offset.
  while (high > charOffset &&
         isFilenamePunctuation(haystack[high])) {
    high--;
  }

  // See if there is a line number here.
  int line = getLineNumberAt(haystack, high+1);

  // Return what we found.
  candidates.push(FileAndLineOpt(
    haystack.substring(low, high-low+1), line));
}


FileAndLineOpt getNearbyFilename(
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset)
{
  SMFileUtil sfu;
  return innerGetNearbyFilename(sfu, candidatePrefixes,
    haystack, charOffset);
}


FileAndLineOpt innerGetNearbyFilename(
  SMFileUtil &sfu,
  ArrayStack<string> const &candidatePrefixes,
  string const &haystack,
  int charOffset)
{
  if (candidatePrefixes.isEmpty()) {
    return FileAndLineOpt();
  }

  // Extract candidate suffixes.
  ArrayStack<FileAndLineOpt> candidateSuffixes;
  getCandidateSuffixes(candidateSuffixes, haystack, charOffset);
  if (candidateSuffixes.isEmpty()) {
    return FileAndLineOpt();
  }

  // Look for a combination that exists on disk.
  for (int prefix=0; prefix < candidatePrefixes.length(); prefix++) {
    for (int suffix=0; suffix < candidateSuffixes.length(); suffix++) {
      string candidateName = sfu.joinFilename(
        candidatePrefixes[prefix], candidateSuffixes[suffix].m_filename);
      candidateName = sfu.collapseDots(candidateName);
      if (sfu.absolutePathExists(candidateName)) {
        return FileAndLineOpt(candidateName,
                              candidateSuffixes[suffix].m_line);
      }
    }
  }

  // No combination exists.  Is the first candidate suffix absolute?
  if (sfu.isAbsolutePath(candidateSuffixes[0].m_filename)) {
    // Yes, return it by itself.
    return candidateSuffixes[0];
  }
  else {
    // Return the first prefix+suffix.
    return FileAndLineOpt(
      sfu.joinFilename(candidatePrefixes[0],
                       candidateSuffixes[0].m_filename), 0);
  }
}


// EOF
