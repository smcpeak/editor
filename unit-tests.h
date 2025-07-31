// unit-tests.h
// Prototypes for the various unit test functions.

// See license.txt for copyright and terms of use.

#ifndef EDITOR_UNIT_TESTS_H
#define EDITOR_UNIT_TESTS_H

#include "smbase/sm-span.h"            // smbase::Span


// Span of command line arguments.  The first element is the first
// argument, not the program name.
typedef smbase::Span<char const * const> CmdlineArgsSpan;


// Prototypes of tests defined in various *-test.cc files.
void test_c_hilite(CmdlineArgsSpan args);
void test_hashcomment_hilite(CmdlineArgsSpan args);
void test_lsp_client(CmdlineArgsSpan args);
void test_lsp_data(CmdlineArgsSpan args);
void test_lsp_manager(CmdlineArgsSpan args);
void test_makefile_hilite(CmdlineArgsSpan args);
void test_ocaml_hilite(CmdlineArgsSpan args);
void test_python_hilite(CmdlineArgsSpan args);
void test_uri_util(CmdlineArgsSpan args);


#endif // EDITOR_UNIT_TESTS_H
