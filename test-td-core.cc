// test-td-core.cc
// Tests for 'td-core' module.

#include "td-core.h"                   // module to test

#include "autofile.h"                  // AutoFILE
#include "ckheap.h"                    // malloc_stats
#include "test.h"                      // USUAL_MAIN

#include <assert.h>                    // assert
#include <stdlib.h>                    // system


static void testAtomicRead()
{
  // Write a file that spans several blocks.
  {
    AutoFILE fp("td-core.tmp", "wb");
    for (int i=0; i < 1000; i++) {
      fputs("                                       \n", fp);  // 40 bytes
    }
  }

  // Read it.
  TextDocumentCore core;
  core.readFile("td-core.tmp");
  xassert(core.numLines() == 1001);

  // Read it again with an injected error.
  try {
    TextDocumentCore::s_injectedErrorCountdown = 10000;
    cout << "This should throw:" << endl;
    core.readFile("td-core.tmp");
    assert(!"should not get here");
  }
  catch (xBase &x) {
    // As expected.
  }

  // Should have consumed this.
  xassert(TextDocumentCore::s_injectedErrorCountdown == 0);

  // Confirm that the original contents are still there.
  xassert(core.numLines() == 1001);

  remove("td-core.tmp");
}


static void entry()
{
  for (int looper=0; looper<2; looper++) {
    printf("stats before:\n");
    malloc_stats();

    // build a text file
    {
      AutoFILE fp("td-core.tmp", "w");

      for (int i=0; i<2; i++) {
        for (int j=0; j<53; j++) {
          for (int k=0; k<j; k++) {
            fputc('0' + (k%10), fp);
          }
          fputc('\n', fp);
        }
      }
    }

    {
      // Read it as a text document.
      TextDocumentCore doc;
      doc.readFile("td-core.tmp");

      // dump its repr
      //doc.dumpRepresentation();

      // write it out again
      doc.writeFile("td-core.tmp2");

      printf("stats before dealloc:\n");
      malloc_stats();

      printf("\nbuffer mem usage stats:\n");
      doc.printMemStats();
    }

    // make sure they're the same
    if (system("diff td-core.tmp td-core.tmp2") != 0) {
      xbase("the files were different!\n");
    }

    // ok
    system("ls -l td-core.tmp");
    remove("td-core.tmp");
    remove("td-core.tmp2");

    printf("stats after:\n");
    malloc_stats();
  }

  {
    printf("reading td-core.cc ...\n");
    TextDocumentCore doc;
    doc.readFile("td-core.cc");
    doc.printMemStats();
  }

  testAtomicRead();

  printf("stats after:\n");
  malloc_stats();

  printf("\ntd-core is ok\n");
}

USUAL_MAIN


// EOF
