# .gdbinit  -*- sh -*-

dir ../smbase
dir ../smqtutil
 
#file qtbdffont
#file ./editor
#file ./test-justify
#file c_hilite
file ./buffer
#set args -tr setView

#set args tmp

break breaker
break abort
break main

#break editor.cc:1554

run
