# .gdbinit  -*- sh -*-

dir ../smbase
dir ../smqtutil
 
#file qtbdffont
file ./editor
#file c_hilite
#set args -tr setView

set args tmp

break breaker
break abort
break main

#break editor.cc:1554

run
