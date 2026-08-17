// lua-hilite-test.cc
// Test code for lua_hilite.{h,lex}.

#include "unit-tests.h"                // decl for my entry point

#include "lua_hilite.h"                // module to test

#include "td-editor.h"                 // TextDocumentAndEditor


// Called from unit-tests.cc.
void test_lua_hilite(CmdlineArgsSpan args)
{
  TextDocumentAndEditor tde;
  Lua_Highlighter hi(tde.getDocument()->getCore());
  testHighlighter(hi, tde, "test/highlight/lua1.lua");
}


// EOF
