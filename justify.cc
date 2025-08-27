// justify.cc
// code for justify.h

#include "justify.h"                   // this module

#include "line-index.h"                // LineIndex
#include "td-editor.h"                 // TextDocumentEditor

#include "smqtutil/qtutil.h"           // toString(QString)

#include "smbase/str.h"                // stringBuilder
#include "smbase/stringb.h"            // stringb

#include <QRegularExpression>

#include <string>                      // std::string
#include <vector>                      // std::vector


// Return true if 'subject' is 'prefix' plus some non-empty suffix.
static bool properStartsWith(
  std::string const &subject, std::string const &prefix)
{
  if (subject.length() <= prefix.length()) {
    return false;
  }

  int len = prefix.length();
  for (int i=0; i < len; i++) {
    if (subject[i] != prefix[i]) {
      return false;
    }
  }

  return true;
}


// Return true if 'c' is punctuation normally found at the end of a
// sentence.  When we have to synthesize space between words, rather
// than copying it, this will determine whether we insert one space or
// two.
static bool isSentenceEnd(char c)
{
  return c == '.' || c == '?' || c == '!';
}


void justifyTextLines(
  std::vector<std::string> &justifiedContent,
  std::vector<std::string> const &originalContent,
  int desiredWidth)
{
  // Line being built.
  //
  // This use of `stringBuilder` is not trivially replaceable with
  // `std::ostringstream` since it queries the length of the string as
  // it is being built.
  //
  stringBuilder curLine;

  // Process all input lines.
  for (std::size_t i=0; i < originalContent.size(); i++) {
    char const *p = originalContent[i].c_str();

    // Loop over words in 'p'.
    while (true) {
      // Leaving 'p' to point at the start of the whitespace before the
      // word, set 'q' to point at the start of the word itself.
      char const *q = p;
      while (*q == ' ') {
        q++;
      }

      if (*q == 0) {
        // No more words in this line.
        break;
      }

      // Advance 'r' to one past the last character in the word.
      char const *r = q+1;
      while (r[0] && r[0] != ' ') {
        r++;
      }

      // How many spaces go before this word?
      int spaces = q-p;
      if (spaces == 0) {
        // Are we at a sentence boundary?
        if (curLine.length() > 0 &&
            isSentenceEnd(curLine[curLine.length()-1])) {
          spaces = 2;
        }
        else {
          spaces = 1;
        }
      }

      // Would adding this word make the line too long?
      if (curLine.length() + spaces + (r-q) > desiredWidth) {
        // Yes, emit the existing line and start a new one.
        if (!curLine.isempty()) {
          justifiedContent.push_back(curLine);
          curLine.clear();
        }
        curLine.append(q, r-q);
      }
      else {
        // No, can add it.
        if (!curLine.isempty()) {
          while (spaces--) {
            curLine << ' ';
          }
        }
        curLine.append(q, r-q);
      }

      p = r;
    } // loop over input words
  } // loop over input lines

  // Emit the partial line if not empty.
  if (!curLine.isempty()) {
    justifiedContent.push_back(curLine);
  }
}


// Calculate the number of columns that 'prefix' will occupy.
// Currently, aside from just counting characters, this routine treates
// all tabs as being 8 columns wide.  It could in the future also
// account for UTF-8 characters, although that requires the editor
// itself to handle those.
static int prefixColumnWidth(std::string const &prefix)
{
  int width = 0;
  for (char const *p = prefix.c_str(); *p; ++p) {
    if (*p == '\t') {
      width += 8;
    }
    else {
      ++width;
    }
  }
  return width;
}


bool justifyNearLine(TextDocumentEditor &tde, LineIndex originLineNumber, int desiredWidth)
{
  std::string startLine = tde.getWholeLineString(originLineNumber);

  // Split the line into a prefix of whitespace and framing punctuation,
  // and a suffix with alphanumeric content.  In a programming language,
  // the prefix is intended to be the comment symbol and indentation.
  // In plain text, the prefix may be empty.
  QRegularExpression re("^([^a-zA-Z'\"0-9`$()_]*)(.*)$");
  xassert(re.isValid());
  QRegularExpressionMatch match = re.match(toQString(startLine));
  if (!match.hasMatch()) {
    return false;
  }
  if (match.captured(2).isEmpty()) {
    // No content.  (Note: I cannot get rid of this test by changing
    // the regex to end with ".+" instead of ".*" because I want the
    // prefix to be as long as possible, whereas with ".+" the regex
    // engine might choose to move a prefix character into the
    // content text group.)
    return false;
  }

  // Grab the prefix string.
  std::string prefix = toString(match.captured(1));

  // Look for adjacent lines that start with the same prefix and have
  // some content after it.
  LineIndex upperEdge = originLineNumber;
  while (upperEdge.isPositive()) {
    if (properStartsWith(tde.getWholeLineString(upperEdge.pred()), prefix)) {
      --upperEdge;
    }
    else {
      break;
    }
  }
  LineIndex lowerEdge = originLineNumber;
  while (lowerEdge.succ() < tde.numLines()) {
    if (properStartsWith(tde.getWholeLineString(lowerEdge.succ()), prefix)) {
      ++lowerEdge;
    }
    else {
      break;
    }
  }

  // Put all the content into a sequence of lines.
  std::vector<std::string> originalContent;
  for (LineIndex i=upperEdge; i <= lowerEdge; ++i) {
    std::string line = tde.getWholeLineString(i);
    originalContent.push_back(line.substr(prefix.size()));
  }

  // Reformat it.
  std::vector<std::string> justifiedContent;
  justifyTextLines(justifiedContent, originalContent,
    desiredWidth - prefixColumnWidth(prefix));

  // TODO: If the original and justified content are the same, do not
  // actually perform the replacement.

  // Replace the content.
  tde.deleteTextLRange(
    TextLCoord(upperEdge, 0),
    TextLCoord(lowerEdge.succ(), 0));
  for (std::size_t i=0; i < justifiedContent.size(); i++) {
    tde.insertString(stringb(prefix << justifiedContent[i] << '\n'));
  }

  return true;
}


// EOF
