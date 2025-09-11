// language-test.cc
// Syntax example for testing LSP services.

#include "language-test.h"             // this module

#include <iostream>

int x;

// Simple, isolated syntax error example.
y                                      // diagnostic

int z;

struct C {
  int m_one;
  int m_two;
  int m_three;

  void method1(int);
  void method2(float, char *p = nullptr);
};

void bar(int);
void bar(float);

// Distinct declaration to find.
void foo(C &c);

// Definition to find.
void foo(C &c)
{
  // Ambiguous lookup error with two candidates.
  bar(c);                              // diagnostic

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
  g_var4 = 4;
}

// `lsp-test-server.py` responds with a malformed URI when asked for all
// occurrences of the following symbol.
int has_malformed_uri_occurrence;

// This one has an out of bounds decl location.
int has_bad_decl_location;

// Has a bad decl file name.
int has_bad_decl_file;

// EOF
