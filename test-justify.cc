// test-justify.cc
// Tests for 'justify' module.

#include "justify.h"                   // module to test

#include <assert.h>                    // assert


static void print(char const *label, ArrayStack<string> const &arr)
{
  cout << label << " (" << arr.length() << "):" << endl;
  for (int i=0; i < arr.length(); i++) {
    cout << "  " << arr[i] << endl;
  }
}


static void testOneJustifyTextLines(
  char const **in, int inSize,
  char const **out, int outSize,
  int desiredWidth)
{
  ArrayStack<string> original;
  for (int i=0; i < inSize; i++) {
    original.push(in[i]);
  }

  ArrayStack<string> expect;
  for (int i=0; i < outSize; i++) {
    expect.push(out[i]);
  }

  ArrayStack<string> actual;
  justifyTextLines(actual, original, desiredWidth);

  if (expect != actual) {
    cout << "desiredWidth: " << desiredWidth << endl;
    print("original", original);
    print("expect", expect);
    print("actual", actual);
    assert(!"justifyTextLines test failure");
  }

  // Confirm that indentation is idempotent.
  actual.clear();
  justifyTextLines(actual, expect, desiredWidth);

  if (expect != actual) {
    cout << "desiredWidth: " << desiredWidth << endl;
    print("expect", expect);
    print("actual", actual);
    assert(!"justifyTextLines idempotence test failure");
  }
}


static void testJustifyTextLines()
{
  #define TEST_ONE(in, out, width) \
    testOneJustifyTextLines(in, TABLESIZE(in), out, TABLESIZE(out), width) /* user ; */

  #define TEST_MULTI(width) \
    TEST_ONE(in, out, width); \
    TEST_ONE(in2, out, width); \
    TEST_ONE(in3, out, width); \
    ((void)0) /* user ; */

  {
    char const *in[] = {
      "a b c d e f g h i",
    };
    char const *in2[] = {
      "a b c d",
      "e f g h i",
    };
    char const *in3[] = {
      "a b c d  ",
      "e f g h i",
    };

    {
      char const *out[] = {
        "a",
        "b",
        "c",
        "d",
        "e",
        "f",
        "g",
        "h",
        "i",
      };
      TEST_MULTI(-1);
      TEST_MULTI(0);
      TEST_MULTI(1);
      TEST_MULTI(2);
    }

    {
      char const *out[] = {
        "a b",
        "c d",
        "e f",
        "g h",
        "i",
      };
      TEST_MULTI(3);
      TEST_MULTI(4);
    }

    {
      char const *out[] = {
        "a b c",
        "d e f",
        "g h i",
      };
      TEST_MULTI(5);
      TEST_MULTI(6);
    }

    {
      char const *out[] = {
        "a b c d",
        "e f g h",
        "i",
      };
      TEST_MULTI(7);
      TEST_MULTI(8);
    }

    {
      char const *out[] = {
        "a b c d e f g h i",
      };
      TEST_MULTI(17);
      TEST_MULTI(18);
    }
  }

  #undef TEST_MULTI
  #define TEST_MULTI(width) \
    TEST_ONE(in, out, width); \
    ((void)0) /* user ; */

  {
    char const *in[] = {
      "one two three four five six seven eight nine  ten eleven twelve",
    };

    {
      char const *out[] = {
        "one two",
        "three",
        "four",
        "five",
        "six",
        "seven",
        "eight",
        "nine",
        "ten",
        "eleven",
        "twelve",
      };
      TEST_MULTI(7);
    }

    {
      char const *out[] = {
        "one two",
        "three four",
        "five six",
        "seven",
        "eight nine",
        "ten eleven",
        "twelve",
      };
      TEST_MULTI(10);
    }

    {
      char const *out[] = {
        "one two three four",
        "five six seven eight",
        "nine  ten eleven",
        "twelve",
      };
      TEST_MULTI(20);
    }
  }

  {
    char const *in[] = {
      "one. two three four. five six seven eight nine.  ten eleven. twelve",
    };

    {
      char const *out[] = {
        "one.",
        "two",
        "three",
        "four.",
        "five",
        "six",
        "seven",
        "eight",
        "nine.",
        "ten",
        "eleven.",
        "twelve",
      };
      TEST_MULTI(7);
    }

    {
      char const *out[] = {
        "one. two",
        "three",
        "four. five",
        "six seven",
        "eight",
        "nine.  ten",
        "eleven.",
        "twelve",
      };
      TEST_MULTI(10);
    }

    {
      char const *out[] = {
        "one. two three four.",
        "five six seven eight",
        "nine.  ten eleven.",
        "twelve",
      };
      TEST_MULTI(20);
    }
  }

  #undef TEST_MULTI
  #undef TEST_ONE
}


static string docToString(TextDocumentEditor const &d)
{
  int lastLine = d.numLines()-1;
  return d.getTextRange(TextCoord(0,0),
    TextCoord(lastLine, d.lineLength(lastLine)));
}


static bool equalDocuments(TextDocumentEditor const &d1,
                           TextDocumentEditor const &d2)
{
  string b1s = docToString(d1);
  string b2s = docToString(d2);
  return b1s == b2s;
}


static void print(char const *label, TextDocumentEditor const &tde)
{
  cout << label << ":\n";
  tde.doc()->getCore().dumpRepresentation();
}


static void testOneJustifyNearLine(
  char const **in, int inSize,
  char const **out, int outSize,
  int originLine, int desiredWidth)
{
  TextDocumentAndEditor original;
  for (int i=0; i < inSize; i++) {
    original.insertText(in[i]);
    original.insertText("\n");
  }

  TextDocumentAndEditor expect;
  for (int i=0; i < outSize; i++) {
    expect.insertText(out[i]);
    expect.insertText("\n");
  }

  TextDocumentAndEditor actual;
  for (int i=0; i < inSize; i++) {
    actual.insertText(in[i]);
    actual.insertText("\n");
  }

  justifyNearLine(actual, originLine, desiredWidth);

  if (!equalDocuments(expect, actual)) {
    cout << "originLine: " << originLine << endl;
    cout << "desiredWidth: " << desiredWidth << endl;
    print("original", original);
    print("expect", expect);
    print("actual", actual);
    assert(!"justifyTextLines test failure");
  }
}


static void testJustifyNearLine()
{
  #define TEST_ONE(in, out, origin, width) \
    testOneJustifyNearLine(in, TABLESIZE(in), out, TABLESIZE(out), \
                           origin, width) /* user ; */

  {
    char const *in[] = {
      "// one two three.  four five six seven eight nine",
      "// ten eleven",
      "// twelve",
    };

    {
      char const *out[] = {
        //              V
        "// one two",
        "// three.  four",
        "// five six",
        "// seven eight",
        "// nine ten",
        "// eleven",
        "// twelve",
      };

      TEST_ONE(in, out, 0, 15);
      TEST_ONE(in, out, 1, 15);
      TEST_ONE(in, out, 2, 15);
    }

    {
      char const *out[] = {
        //                   V
        "// one two three.",
        "// four five six",
        "// seven eight nine",
        "// ten eleven twelve",
      };

      TEST_ONE(in, out, 0, 20);
      TEST_ONE(in, out, 1, 20);
      TEST_ONE(in, out, 2, 20);
    }

    {
      char const *out[] = {
        //                             V
        "// one two three.  four five",
        "// six seven eight nine ten",
        "// eleven twelve",
      };

      TEST_ONE(in, out, 1, 30);
    }
  }

  {
    char const *in[] = {
      "// one two three.  four five six seven eight nine",
      "// ",
      "// ten eleven",
      "// twelve",
    };

    {
      char const *out[] = {
        //              V
        "// one two",
        "// three.  four",
        "// five six",
        "// seven eight",
        "// nine",
        "// ",
        "// ten eleven",
        "// twelve",
      };

      TEST_ONE(in, out, 0, 15);
    }

    TEST_ONE(in, in, 1, 15);
    TEST_ONE(in, in, 2, 15);
    TEST_ONE(in, in, 3, 15);

    {
      char const *out[] = {
        //                   V
        "// one two three.",
        "// four five six",
        "// seven eight nine",
        "// ",
        "// ten eleven",
        "// twelve",
      };

      TEST_ONE(in, out, 0, 20);
    }

    TEST_ONE(in, in, 1, 20);

    {
      char const *out[] = {
        "// one two three.  four five six seven eight nine",
        "// ",
        "// ten eleven twelve",
      };

      TEST_ONE(in, out, 2, 20);
      TEST_ONE(in, out, 3, 20);
    }
  }

  {
    char const *in[] = {
      "one two three.  four five six seven eight nine",
      "ten eleven",
      "twelve",
    };

    {
      char const *out[] = {
        //              V
        "one two three.",
        "four five six",
        "seven eight",
        "nine ten eleven",
        "twelve",
      };

      TEST_ONE(in, out, 0, 15);
      TEST_ONE(in, out, 1, 15);
      TEST_ONE(in, out, 2, 15);
    }
  }

  {
    char const *in[] = {
      "one two three.  four five six seven eight nine",
      "",
      "ten eleven",
      "twelve",
    };

    {
      char const *out[] = {
        //              V
        "one two three.",
        "four five six",
        "seven eight",
        "nine",
        "",
        "ten eleven",
        "twelve",
      };

      TEST_ONE(in, out, 0, 15);
    }

    TEST_ONE(in, in, 1, 15);

    {
      char const *out[] = {
        //              V
        "one two three.  four five six seven eight nine",
        "",
        "ten eleven",
        "twelve",
      };

      TEST_ONE(in, out, 2, 15);
      TEST_ONE(in, out, 3, 15);
    }
  }

  #undef TEST_ONE
}


int main()
{
  testJustifyTextLines();
  testJustifyNearLine();

  cout << "test-justify PASSED\n";
  return 0;
}


// EOF
