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


bool justifyNearLine(Buffer &buf, int originLineNumber, int desiredWidth)
{
  string startLine = buf.getWholeLine(originLineNumber);

  // Does this look like a comment line?
  QRegularExpression re("^(\\s*)(//|#|;+|--)(\\s*)(\\S.*)$");
  QRegularExpressionMatch match = re.match(toQString(startLine));
  if (!match.hasMatch()) {
    return false;
  }

  // Assemble the prefix string.
  QString prefixQString =
    match.captured(1) + match.captured(2) + match.captured(3);
  string prefix = toString(prefixQString);

  // Look for adjacent lines that start with the same prefix and have
  // some content after it.
  int upperEdge = originLineNumber;
  while (upperEdge-1 >= 0) {
    if (properStartsWith(buf.getWholeLine(upperEdge-1), prefix)) {
      upperEdge--;
    }
    else {
      break;
    }
  }
  int lowerEdge = originLineNumber;
  while (lowerEdge+1 < buf.numLines()) {
    if (properStartsWith(buf.getWholeLine(lowerEdge+1), prefix)) {
      lowerEdge++;
    }
    else {
      break;
    }
  }

  // Put all the content into a sequence of lines.
  ArrayStack<string> originalContent;
  for (int i=upperEdge; i <= lowerEdge; i++) {
    string line = buf.getWholeLine(i);
    originalContent.push(line.c_str() + prefix.length());
  }

  // Reformat it.
  ArrayStack<string> justifiedContent;
  justifyTextLines(justifiedContent, originalContent,
    desiredWidth - prefix.length());

  // Replace the content.
  buf.deleteTextRange(upperEdge, 0, lowerEdge+1, 0);
  for (int i=0; i < justifiedContent.length(); i++) {
    stringBuilder sb;
    sb << prefix << justifiedContent[i] << '\n';
    buf.insertText(sb.c_str());
  }

  return true;
}


// EOF
