// buffer.cc
// code for buffer.h

#include "buffer.h"        // this module
#include "strutil.h"       // encodeWithEscapes
#include "syserr.h"        // xsyserror

#include <sys/types.h>     // ?
#include <sys/stat.h>      // O_RDONLY, etc.
#include <fcntl.h>         // open, close
#include <unistd.h>        // read, write
#include <stdio.h>         // FILE, etc.
#include <stdlib.h>        // system

Buffer::Buffer()
  : lines()
{}

Buffer::~Buffer()
{
  // 'lines' automatically deallocs itself
}


void Buffer::readFile(char const *fname)
{
  // use raw 'read' calls and my own buffer to avoid unnecessary
  // overhead from FILE functions; a large buffer will minimize
  // the IPC (interprocess communication)

  enum { BUFSIZE=32 };     // DEBUG: small so I can easily test boundary issues
  char buf[BUFSIZE];

  int fd = open(fname, O_RDONLY);
  if (fd < 0) {
    xsyserror("open");
  }

  // prepare to access 'lines;
  lines.growWithMargin(1);

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
        lines[curLine].realloc(curCol + (end-start));
        memcpy(lines[curLine].data + curCol, start, end-start);
        
        // note the increased line length
        lines[curLine].size = curCol + (end-start);

        // move our insertion point
        curLine++;
        curCol = 0;
        lines.growWithMargin(curLine+1);    // so lines[curLine] is always valid

        // move our read-from-next point
        start = end+1;                      // skip newline char
      }

      else {
        // we ran into the end of the buffer; add this text
        // but don't move to the next line
        lines[curLine].growWithMargin(curCol + (end-start));
        memcpy(lines[curLine].data + curCol, start, end-start);

        // note the increased line length
        lines[curLine].size = curCol + (end-start);

        // move insertion point w/in the line
        curCol += end-start;

        // and read-from-next point (will cause us to
        // break the inner loop)
        start = end;
      }
    }
  }

  // finish the last line; catches EOF & EOL at buffer boundary
  lines[curLine].realloc(curCol);

  // record the total number of lines
  lines.size = curLine+1;

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

  for (int line=0; line<lines.size; line++) {
    if (1 != fwrite(lines[line].data, lines[line].size, 1, fp)) {
      xsyserror("write");
    }

    if (line+1 < lines.size) {
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
 
  for (int i=0; i<lines.size; i++) {
    printf("line %d: \"%s\"\n", i, 
           encodeWithEscapes(lines[i].data, lines[i].size).pchar());
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
    for (int j=0; j<73; j++) {
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
  remove("buffer.tmp");
  //remove("buffer.tmp2");

  printf("stats after:\n");
  malloc_stats();

  return 0;
}

USUAL_MAIN


#endif // TEST_BUFFER
