// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module
#include "textline.h"      // TextLine
#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror
#include "ckheap.h"        // checkHeap

#include <sys/types.h>     // ?
#include <sys/stat.h>      // O_RDONLY, etc.
#include <fcntl.h>         // open, close
#include <unistd.h>        // read, write
#include <stdio.h>         // FILE, etc.
#include <stdlib.h>        // system


Buffer::Buffer()
{
  lines = NULL;
  numLines = 0;
  linesAllocated = 0;
}

Buffer::~Buffer()
{
  for (int i=0; i<numLines; i++) {
    lines[i].dealloc();
  }
}
                                                       

// adjustable parameters (don't parenthesize)
#define LINES_SHRINK_RATIO 1 / 4
#define LINES_GROW_RATIO 2
#define LINES_GROW_STEP 20


// always uses a margin calculation
void Buffer::setNumLines(int newLines)
{
  // dealloc any lines now not part of the buffer
  int i;
  for (i=newLines; i<numLines; i++) {
    lines[i].dealloc();
  }

  if (linesAllocated < newLines  ||
      newLines < linesAllocated * LINES_SHRINK_RATIO) {
    int newAllocated = newLines * LINES_GROW_RATIO + LINES_GROW_STEP;

    // realloc and copy
    TextLine *newArray = new TextLine[newAllocated];

    // copy common prefix
    int preserved = min(numLines, newLines);
    memcpy(newArray, lines, preserved * sizeof(TextLine));

    // dealloc old array
    if (lines) {
      delete[] lines;    // does *not* call dtors!  essential!
    }

    // reassign to new
    lines = newArray;
    linesAllocated = newAllocated;
  }

  // init any lines that are now part of the buffer
  for (i=numLines; i<newLines; i++) {
    lines[i].init();
  }

  // set length
  numLines = newLines;
}


void Buffer::readFile(char const *fname)
{
  // use raw 'read' calls and my own buffer to avoid unnecessary
  // overhead from FILE functions; a large buffer will minimize
  // the IPC (interprocess communication)

  //enum { BUFSIZE=16 };       // DEBUG: small so I can easily test boundary issues
  enum { BUFSIZE=8192 };
  char buf[BUFSIZE];

  int fd = open(fname, O_RDONLY);
  if (fd < 0) {
    xsyserror("open");
  }

  int curLine = 0;
  int curCol = 0;
  for (;;) {
    int len = read(fd, buf, BUFSIZE);
    if (len == 0) {
      break;    // EOF
    }
    if (len < 0) {
      xsyserror("read");
    }

    // mark the start of the new text to assimilate
    char *start = buf;

    // loop over all line fragments in the buffer
    while (start < buf+len) {
      // look for a newline
      char *end = start;
      while (*end != '\n' &&
             end < buf+len) {
        end++;
      }

      if (end < buf+len) {
        // we found a newline; add this text to the buffer
        setNumLines(curLine+1);
        if (curCol == 0) {
          // set a complete line, with no margin
          lines[curLine].setText(start, end-start);
        }
        else {
          // append to what's there, and take the hit of the margin
          lines[curLine].insert(curCol, start, end-start);
        }

        // move our insertion point
        curLine++;
        curCol = 0;

        // move our read-from-next point
        start = end+1;                      // skip newline char
      }

      else {
        // we ran into the end of the buffer; add this text
        // but don't move to the next line
        setNumLines(curLine+1);
        lines[curLine].insert(curCol, start, end-start);

        // move insertion point w/in the line
        curCol += end-start;

        // and read-from-next point (will cause us to
        // break the inner loop)
        start = end;
      }
    }
  }

  // record the total number of lines
  // (also catches EOF & EOL at buffer boundary, which is
  // otherwise tricky)
  setNumLines(curLine+1);

  if (close(fd) < 0) {
    xsyserror("close");
  }
}


void Buffer::writeFile(char const *fname)
{
  // use FILE ops for writing so we don't do an IPC for
  // each line; in this case automatic buffering is good

  FILE *fp = fopen(fname, "w");
  if (!fp) {
    xsyserror("open");
  }

  for (int line=0; line<numLines; line++) {
    if (1 != fwrite(lines[line].getText(), 
                    lines[line].getLength(), 1, fp)) {
      xsyserror("write");
    }

    if (line+1 < numLines) {
      // newline separator
      if (1 != fwrite("\n", 1, 1, fp)) {
        xsyserror("write");
      }
    }
  }

  if (0 != fclose(fp)) {
    xsyserror("close");
  }
}


void Buffer::dumpRepresentation()
{
  printf("-- buffer --\n");

  for (int i=0; i<numLines; i++) {
    printf("line %d: [%d/%d] \"%s\"\n",
           i, lines[i].getLength(), lines[i]._please_getAllocated(),
           encodeWithEscapes(lines[i].getText(),
                             lines[i].getLength()).pchar());
  }
}

  
// --------------------- test code -----------------------
#ifdef TEST_BUFFER

#include "test.h"      // USUAL_MAIN
#include "malloc.h"    // malloc_stats

int entry()
{
  printf("stats before:\n");
  malloc_stats();

  // build a text file
  FILE *fp = fopen("buffer.tmp", "w");
  if (!fp) {
    xsyserror("open");
  }

  for (int i=0; i<5; i++) {
    for (int j=0; j<53; j++) {
      for (int k=0; k<j; k++) {
        fputc('0' + (k%10), fp);
      }
      fputc('\n', fp);
    }
  }
  fprintf(fp, "last line no newline");
  
  fclose(fp);

  {
    // read it as a buffer
    Buffer buf;
    buf.readFile("buffer.tmp");

    // dump its repr
    buf.dumpRepresentation();

    // write it out again
    buf.writeFile("buffer.tmp2");

    printf("stats before dealloc:\n");
    malloc_stats();
  }

  // make sure they're the same
  if (system("diff buffer.tmp buffer.tmp2 >/dev/null") != 0) {
    printf("the files were different!\n");
    return 2;
  }
  
  // ok
  system("ls -l buffer.tmp");
  remove("buffer.tmp");
  remove("buffer.tmp2");

  printf("stats after:\n");
  malloc_stats();

  return 0;
}

USUAL_MAIN


#endif // TEST_BUFFER
