// c1.c
// Main example file for testing C/C++ highlighting.

// Main list of keywords.
asm auto break bool case catch cdecl char class const const_cast continue
default delete do double dynamic_cast else enum explicit export extern
float for friend goto if inline int long mutable namespace new operator
pascal private protected public register reinterpret_cast return short
signed sizeof static static_cast struct switch template this throw
try typedef typeid typename union unsigned using virtual void volatile
wchar_t while

// GNU keywords
__attribute__
__extension__
__typeof__
typeof

/* operators, punctuators */
( ) [ ] -> :: .  !  ~ + - ++ -- & * .* ->* / % << >> < <= > >= == != ^
| && || ?  : = *= /= %= += -= &= ^= |= <<= >>= , ...  ; { }

/* my extension */
==>

/* to avoid backing up; treat as two consecutive "." */
..

/* special values */
true false null TRUE FALSE NULL

/* additional words to highlight as keywords */
string

identifiers: a z _ A Z a1 z1 _1 _A _Z __blah_blah

decimal:
  0
  0l 0L 0ll 0LL 0lL 0Ll
  0u 0ul 0uL 0ull 0uLL 0ulL 0uLl
  0U 0Ul 0UL 0Ull 0ULL 0UlL 0ULl
  1 10 19 19u 19LL
hex: 0x0 0x9 0XA 0XF 0xfull
octal: 00 01 07 0777
treated as decimal, not right: 079
float:
  0.0 1.1 1.2e3 1.2e+3f 12.3e-4l 12.3L
  0. 1. 1.e3 1.e+3f 12.e-4l 12.L
  .0 .1 .2e3 .2e+3f .3e-4l .3L

  s = "string \" split \
       across two lines";
  s = "string \" split \
       across three \
       lines";
  s = "one line \" string";
  s = "unterminated \" string
  this is normal text

charlit:
  '' 'a' '\'' '\"' 'abcdef'
  L'' L'a' L'\'' L'\"' L'abcdef'
  'charlit ending in backslash \
  L'charlit ending in backslash \
  'charlit followed by backslash newline is error'\
  normal text

// C++ comment
text then // C++ comment
// C++ comment containing /* C comment */ still comment
can // you continue \
       a C++ comment? yes! so this is still a comment
       now we are outside the comment
// C++ comment \
continued \
across three lines
back to normal

/* C comment */
normal text /* C comment */ normal text
cannot /* nest /* C */ comments
can /* have * / in C */ comment
can /* have // in C */ comment

this /* C comment
goes onto */ the next line

normal
/* C comment
   across
   three lines */ normal

/* C comment with \
   a backslash at the end \
   of each line */ normal

#define has // C++ comment

#define has // continued C++ \
               comment

#define FOO macro that \
            spans two lines
int foo;
#define FOO macro that \
            spans \
            three lines lines
int blah;
#define foo macro with /* C comment */ inside it
#define foo continued \
            macro with /* C comment */ inside it
#define foo continued \
            macro ending with // C++ comment (not handled)
