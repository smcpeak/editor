# Makefile

all: editor

ccflags = -g -Wall
includes = -I/usr/X11/include
libraries = -L/usr/X11/lib -lXm -lXt
compile = g++ -c ${ccflags} ${defines} ${includes}
ccompile = gcc -c ${ccflags} ${defines} ${includes}
link = g++ ${ccflags} ${defines} ${includes}
linkend = ${libraries}
makelib = ar -r

%.o : %.cc
	${compile} $<

%.o : %.c
	${ccompile} $<

# compile an executable
%.exe : %.o
	${link} -o $@ $< ${linkend}

push: push.o
	${link} -o push push.o ${linkend}

origpush: orig.push.o
	${link} -o origpush orig.push.o ${linkend}

rowcol: rowcol.o
	${link} -o rowcol rowcol.o ${linkend}

hello: hello.o
	${link} -o hello hello.o -L/usr/X11/lib -lX11

editor: editor.o
	${link} -o editor editor.o -L/usr/X11/lib -lX11
