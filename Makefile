# Makefile

all: buffer editor

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

editor-src = editor.o buffer.o textline.o cursor.o
editor: ${editor-src} buffer.h textline.h cursor.h
	${link} -o editor ${editor-src} ${linkend}

buffer-src = buffer.cc textline.o cursor.o
buffer: array.h ${buffer-src}
	${link} -o buffer -DTEST_BUFFER ${buffer-src} ${linkend}

clean:
	rm -f *.o
	rm editor buffer
