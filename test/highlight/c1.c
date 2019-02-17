// c1.c
#define FOO macro that \
            spans two lines
int foo;
#define FOO macro that \
            spans \
            three lines lines
int blah;
  s = "one line string";
  s = "string split \
       across two lines";
  s = "string split \
       across three \
       lines";
