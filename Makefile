# Makefile for editor

all: buffer editor dialogs

SMBASE := smbase

ccflags = -g -Wall
includes = -I/usr/X11/include -I$(SMBASE)
#libraries = -L$(SMBASE) -l$(SMBASE) -L/usr/X11/lib -lXm -lXt
libraries = -L$(SMBASE) -l$(SMBASE) -L/usr/X11/lib -lX11
compile = g++ -c ${ccflags} ${defines} ${includes}
ccompile = gcc -c ${ccflags} ${defines} ${includes}
link = g++ ${ccflags} ${defines} ${includes}
linkend = ${libraries}
makelib = ar -r

TOCLEAN =

TOCLEAN += *.o *.d
%.o : %.cc
	${compile} -o $@ $<
	@perl $(SMBASE)/depend.pl -o $@ $< $(ccflags) $(defines) $(includes) > $*.d

TOCLEAN += editor
editor-src = editor.o buffer.o textline.o position.o
editor: ${editor-src} buffer.h textline.h position.h
	${link} -o editor ${editor-src} ${linkend}
-include $(editor-src:.o=.d)

TOCLEAN += buffer buffer.tmp
buffer-src = buffer.cc textline.o position.o
buffer: array.h ${buffer-src}
	${link} -o buffer -DTEST_BUFFER ${buffer-src} ${linkend}

TOCLEAN += dialogs
dialogs: dialogs.cc
	${link} -o dialogs dialogs.cc -lXm -lXt ${linkend}

clean:
	rm -f $(TOCLEAN)
