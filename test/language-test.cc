// language-test.cc
// Syntax example for testing LSP services.

#include "language-test.h"             // this module

#include <iostream>

int x;

// Simple, isolated syntax error example.
y

int z;

struct C {
  int m_one;
  int m_two;
  int m_three;
};

void bar(int);
void bar(float);

// Distinct declaration to find.
void foo(C &c);

// Definition to find.
void foo(C &c)
{
  // Ambiguous lookup error with two candidates.
  bar(c);

  // Failed lookup example with a large number of candidates.
  //std::cout << c;

  c.
}

void caller(C &c)
{
  // Call site so we can look up the definition or declaration.
  foo(c);

  // Several more uses so all-uses is interesting.
  foo(c);
  foo(c);
  foo(c);
  foo(c);
  foo(c);
}


void useGlobals()
{
  g_var3 = 3;
}

// EOF
