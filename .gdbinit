# .gdbinit  -*- sh -*-

dir ../smbase
dir ../smqtutil
 
#file qtbdffont
file editor
#file c_hilite
#set args -tr setView

set args editor.h

break breaker
break main

#break editor.cc:1554

run
