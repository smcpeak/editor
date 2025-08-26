// bufferlinesource-test.cc
// Test code for bufferlinesource.

#include "unit-tests.h"                // decl for my entry point

#include "bufferlinesource.h"          // module to test

// editor
#include "td-editor.h"                 // TextDocumentAndEditor

// smbase
#include "smbase/sm-test.h"            // EXPECT_EQ


// Called from unit-tests.cc.
void test_bufferlinesource(CmdlineArgsSpan args)
{
  char const *text =
    "one\n"
    "\n"
    "three\n"
    "four\n"
    "a fairly long line to exercise multiple buffered reads\n"
    "six";    // missing newline

  string textWithNewline = stringb(text << '\n');

  TextDocumentAndEditor tde;
  tde.insertNulTermText(text);

  // For a range of buffer sizes, read out all of the lines using
  // BufferLineSource.  Concatenate them together and expect the
  // result to match 'text'.
  for (int bufSize=1; bufSize < 70; bufSize++) {
    Array<char> buffer(bufSize);
    BufferLineSource bls;
    std::ostringstream sb;
    FOR_EACH_LINE_INDEX_IN(line, tde) {
      bls.beginScan(tde.getDocumentCore(), line);

      int len = bls.fillBuffer(buffer.ptr(), bufSize);
      xassert(bls.lineIsEmpty() == tde.isEmptyLine(line));
      while (len > 0) {
        sb << string(buffer.ptr(), len);
        len = bls.fillBuffer(buffer.ptr(), bufSize);
      }
      xassert(len == 0);      // Should never be negative.
    }

    // What we concatenated by reading from 'bls' should match the
    // original text, except that 'bls' will have synthesized a newline
    // for the last line in the file.
    EXPECT_EQ(sb.str(), textWithNewline);
  }
}


// EOF
