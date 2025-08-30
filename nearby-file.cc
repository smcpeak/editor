// nearby-file.cc
// code for nearby-file.h

#include "nearby-file.h"               // this module

#include "line-number.h"               // LineNumber

#include "smbase/codepoint.h"          // isDecimalDigit, isLetter, etc.
#include "smbase/gdvalue-optional.h"   // gdv::toGDValue(std::optional)
#include "smbase/overflow.h"           // multiplyAddWithOverflowCheck
#include "smbase/sm-file-util.h"       // SMFileUtil
#include "smbase/string-util.h"        // prefixAll, suffixAll, join, doubleQuote
#include "smbase/trace.h"              // TRACE
#include "smbase/vector-util.h"        // vecConvertElements, vecMapElements

#include <optional>                    // std::{nullopt, optional}

using namespace gdv;


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
  return isLetter(c) || isFilenameDigit(c) || c == '.';
}

// True if 'c' is punctuation that commonly appears in text file names,
// including as a directory separator.  A key aspect of these characters
// is they are not expected to be consecutive in a valid file name.
static bool isFilenamePunctuation(char c)
{
  return c == '_' ||
         c == '-' ||
         c == '/' ||
         c == '\\';          // for Windows
}

// True if 'c' is punctuation or '.'.  We discard trailing letters that
// pass this test.
static bool isFilenamePunctuationOrDot(char c)
{
  return isFilenamePunctuation(c) || c == '.';
}

// Return true if 'c' is a character that commonly appears in the name
// of text files.
static bool isFilenameChar(char c)
{
  return isFilenameCore(c) || isFilenamePunctuation(c);
}


// If the characters at 'haystack[i]' look like a line number in the
// form ":$N", return the number, otherwise return 0.
static std::optional<LineNumber>
getLineNumberAt(string const &haystack, int i)
{
  try {
    char const *start = haystack.c_str() + i;
    char const *p = start;
    if (*p == ':') {
      p++;
      int ret = 0;
      while (isFilenameDigit(*p) && (p-start < 10)) {
        ret = multiplyAddWithOverflowCheck(ret, 10, (*p - '0'));
        p++;
      }

      // We should have ended on something other than a digit or letter.
      if (isFilenameCore(*p)) {
        return std::nullopt;
      }

      if (ret > 0) {
        return LineNumber(ret);
      }
    }
  }
  catch (...) {
    // Ignore overflowed arithmetic and fall back to no detection.
  }

  return std::nullopt;
}


// Search at 'charOffset' in 'haystack' for candidate file names.
//
// This interface is slightly busted.  The returned candidates always
// have a local host because I'm only intending to return a file name
// and line number here.
//
// Right now this returns at most one candidate, but I am designing the
// interface to anticipate the ability to return multiple candidates.
// For example, a first candidate might never consider filenames to
// have spaces, but the next candidate might allow spaces, etc.
void getCandidateSuffixes(
  ArrayStack<HostFileAndLineOpt> /*OUT*/ &candidates,
  string const &haystack,
  int charOffset)
{
  candidates.clear();

  // I want this to be usable without too much pre-checking, so just
  // silently discard an invalid offset or empty string.
  if (haystack.empty()) {
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

  // File names do not usually end with punctuation or dots.
  if (isFilenamePunctuationOrDot(haystack[charOffset]) &&
      charOffset == haystackLength-1) {
    return;
  }

  // The cursor should not be on consecutive punctuation if it really is
  // on a file name.
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
         isFilenamePunctuationOrDot(haystack[high])) {
    high--;
  }

  // See if there is a line number here.
  std::optional<LineNumber> line = getLineNumberAt(haystack, high+1);

  // Return what we found.
  candidates.push(HostFileAndLineOpt(
    HostAndResourceName::localFile(haystack.substr(low, high-low+1)),
    line, std::nullopt));
}


// Return an object with the prefix hostname, a file name created by
// joining prefix+suffix, and line/col from the suffix.
static HostFileAndLineOpt joinHFL(
  SMFileUtil &sfu,
  HostAndResourceName const &prefix,
  HostFileAndLineOpt const &suffix)
{
  string joinedFileName = sfu.joinIfRelativeFilename(
    prefix.resourceName(),
    suffix.m_harn.resourceName());
  joinedFileName = sfu.collapseDots(joinedFileName);

  // Take the host from the prefix and the line/col number from the
  // suffix.
  return HostFileAndLineOpt(
    HostAndResourceName(prefix.hostName(), joinedFileName),
    suffix.m_line,
    suffix.m_byteIndex);
}


static HostFileAndLineOpt innerGetNearbyFilename(
  IHFExists &ihfExists,
  ArrayStack<HostAndResourceName> const &candidatePrefixes,
  string const &haystack,
  int charOffset)
{
  SMFileUtil sfu;

  if (candidatePrefixes.isEmpty()) {
    return HostFileAndLineOpt();
  }

  // Extract candidate suffixes.
  ArrayStack<HostFileAndLineOpt> candidateSuffixes;
  getCandidateSuffixes(candidateSuffixes, haystack, charOffset);
  if (candidateSuffixes.isEmpty()) {
    return HostFileAndLineOpt();
  }

  // Look for a combination that exists on disk.
  for (int prefix=0; prefix < candidatePrefixes.length(); prefix++) {
    for (int suffix=0; suffix < candidateSuffixes.length(); suffix++) {
      HostFileAndLineOpt candidate(joinHFL(sfu,
        candidatePrefixes[prefix],
        candidateSuffixes[suffix]));

      if (ihfExists.hfExists(candidate.m_harn)) {
        return candidate;
      }
    }
  }

  // No combination exists.  Return the first prefix+suffix.
  return joinHFL(sfu, candidatePrefixes[0], candidateSuffixes[0]);
}


static string harnToString(HostAndResourceName const &harn)
{
  return harn.toString();
}


HostFileAndLineOpt getNearbyFilename(
  IHFExists &ihfExists,
  ArrayStack<HostAndResourceName> const &candidatePrefixes,
  string const &haystack,
  int charOffset)
{
  HostFileAndLineOpt ret = innerGetNearbyFilename(
    ihfExists,
    candidatePrefixes,
    haystack,
    charOffset);

  TRACE("nearby-file", "getNearbyFilename:\n"
    "  candidatePrefixes:\n" <<
    join(
      suffixAll(
        prefixAll(
          vecConvertElements<std::string>(
            vecMapElements<string>(
              candidatePrefixes.asVector(),
              harnToString)),          // map function
          "    "),                     // prefix
        "\n"),                         // suffix
      "") <<                           // separator
    "  haystack: " << doubleQuote(haystack) << "\n"
    "  charOffset: " << charOffset << "\n"
    "  ret.harn: " << ret.m_harn << "\n"
    "  ret.line: " << toGDValue(ret.m_line));

  return ret;
}


// EOF
