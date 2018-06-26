# .gdbinit  -*- sh -*-

dir ../smbase
dir ../smqtutil
 
#file qtbdffont
file ./editor
#file ./test-justify
#file c_hilite
#file ./buffer
#file ./td

set args -tr objectCount main.h main.cc editor-widget.h editor-widget.cc

#set args tmp

break breaker
break abort
break main

#break editor.cc:1554

run
