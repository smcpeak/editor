// justify.cc
// code for justify.h

#include "justify.h"                   // this module

#include "qtutil.h"                    // toString(QString)

#include <QRegularExpression>


// Return true if 'subject' is 'prefix' plus some non-empty suffix.
static bool properStartsWith(string const &subject, string const &prefix)
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
  ArrayStack<string> &justifiedContent,
  ArrayStack<string> const &originalContent,
  int desiredWidth)
{
  // Line being built.
  stringBuilder curLine;

  // Process all input lines.
  for (int i=0; i < originalContent.size(); i++) {
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
          justifiedContent.push(curLine);
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
    justifiedContent.push(curLine);
  }
}


bool justifyNearLine(TextDocumentEditor &tde, int originLineNumber, int desiredWidth)
{
  string startLine = tde.getWholeLine(originLineNumber);

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
  string prefix = toString(match.captured(1));

  // Look for adjacent lines that start with the same prefix and have
  // some content after it.
  int upperEdge = originLineNumber;
  while (upperEdge-1 >= 0) {
    if (properStartsWith(tde.getWholeLine(upperEdge-1), prefix)) {
      upperEdge--;
    }
    else {
      break;
    }
  }
  int lowerEdge = originLineNumber;
  while (lowerEdge+1 < tde.numLines()) {
    if (properStartsWith(tde.getWholeLine(lowerEdge+1), prefix)) {
      lowerEdge++;
    }
    else {
      break;
    }
  }

  // Put all the content into a sequence of lines.
  ArrayStack<string> originalContent;
  for (int i=upperEdge; i <= lowerEdge; i++) {
    string line = tde.getWholeLine(i);
    originalContent.push(line.c_str() + prefix.length());
  }

  // Reformat it.
  ArrayStack<string> justifiedContent;
  justifyTextLines(justifiedContent, originalContent,
    desiredWidth - prefix.length());

  // Replace the content.
  tde.deleteTextRange(upperEdge, 0, lowerEdge+1, 0);
  for (int i=0; i < justifiedContent.length(); i++) {
    stringBuilder sb;
    sb << prefix << justifiedContent[i] << '\n';
    tde.insertText(sb.c_str());
  }

  return true;
}


// EOF
