# Makefile

all: editor buffer

ccflags = -g -Wall
includes = -I/usr/X11/include -Ismbase
#libraries = -Lsmbase -lsmbase -L/usr/X11/lib -lXm -lXt
libraries = -Lsmbase -lsmbase -L/usr/X11/lib -lX11
compile = g++ -c ${ccflags} ${defines} ${includes}
ccompile = gcc -c ${ccflags} ${defines} ${includes}
link = g++ ${ccflags} ${defines} ${includes}
linkend = ${libraries}
makelib = ar -r

%.o : %.cc
	${compile} $<

%.o : %.c
	${ccompile} $<

editor: editor.o buffer.o
	${link} -o editor editor.o ${linkend}

buffer: buffer.cc array.h textline.o
	${link} -o buffer -DTEST_BUFFER textline.o buffer.cc ${linkend}
