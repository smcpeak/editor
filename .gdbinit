# .gdbinit  -*- sh -*-

dir ../smbase
dir ../smqtutil
 
#file qtbdffont
#file ./editor
#file ./test-justify
#file c_hilite
#file ./buffer
file ./test-td-editor

#set args -tr objectCount td.h

#set args tmp

break breaker
break abort
break main

run
