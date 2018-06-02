# Makefile for editor

# main target
all: comment.yy.cc testgap buffercore historybuf buffer style c_hilite editor


# directories of other software
SMBASE := ../smbase
LIBSMBASE := $(SMBASE)/libsmbase.a

# C++ compiler, etc.
CXX := x86_64-w64-mingw32-g++

# flags for the C and C++ compilers (and preprocessor)
#
# The "-I." is so that "include <FlexLexer.h>" will pull in the
# FlexLexer.h in the currect directory, which is a copy of the
# one that Cygwin put in /usr/include.  I cannot directly use that
# one because the mingw compiler does not look in /usr/include
# since that has the native cygwin library headers.
CCFLAGS := -g -Wall -I. -I$(SMBASE) -Wno-deprecated

# flags for the linker
LDFLAGS := -g -Wall $(LIBSMBASE)


# patterns of files to delete in the 'clean' target; targets below
# add things to this using "+="
TOCLEAN =


# ---------------- pattern rules --------------------
# compile .cc to .o
TOCLEAN += *.o *.d
%.o : %.cc
	$(CXX) -c -o $@ $< $(CCFLAGS)
	@perl $(SMBASE)/depend.pl -o $@ $< $(CCFLAGS) > $*.d


# ---------------- gap test program -----------------
TOCLEAN += testgap
testgap: gap.h testgap.cc
	$(CXX) -o $@ $(CCFLAGS) testgap.cc -g -Wall $(LIBSMBASE)
	./testgap >/dev/null 2>&1


# -------------- buffercore test program ----------------
TOCLEAN += buffercore buffer.tmp
BUFFERCORE_OBJS := buffercore.cc
buffercore: gap.h $(BUFFERCORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_BUFFERCORE $(BUFFERCORE_OBJS) -g -Wall $(LIBSMBASE)
	./buffercore >/dev/null 2>&1

# -------------- historybuf test program ----------------
TOCLEAN += historybuf historybuf.tmp
HISTORYBUF_OBJS := historybuf.cc history.o buffercore.o
historybuf: gap.h $(HISTORYBUF_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_HISTORYBUF $(HISTORYBUF_OBJS) -g -Wall $(LIBSMBASE)
#	./historybuf >/dev/null 2>&1

# -------------- buffer test program ----------------
TOCLEAN += buffer
BUFFER_OBJS := buffer.cc buffercore.o history.o historybuf.o
buffer: gap.h $(BUFFER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_BUFFER $(BUFFER_OBJS) -g -Wall $(LIBSMBASE)
	./buffer >/dev/null 2>&1


# --------------- style test program ----------------
TOCLEAN += style
style: style.h style.cc
	$(CXX) -o $@ $(CCFLAGS) -DTEST_STYLE style.cc -g -Wall $(LIBSMBASE)
	./style >/dev/null


# ------------- highlighting stuff --------------------
# lexer (-b makes lex.backup)
TOCLEAN += comment.yy.cc c_hilite.yy.cc *.lex.backup
%.yy.cc: %.lex %.h
	flex -o$@ -b -P$*_yy $*.lex
	mv lex.backup $*.lex.backup
	head $*.lex.backup
#	mv $@ $*.tmp
#	sed -e 's|class istream;|#include <iostream.h>|' \
#	    -e 's|using namespace std;|using std::cout; using std::cin; using std::istream;|' \
#	  <$*.tmp >$@
#	rm $*.tmp

C_HILITE_OBJS := \
  buffercore.o \
  history.o \
  historybuf.o \
  buffer.o \
  style.o \
  lex_hilite.o \
  bufferlinesource.o \
  comment.yy.o \
  c_hilite.yy.o
#-include $(C_HILITE_OBJS:.o=.d)   # redundant with EDITOR_OBJS

TOCLEAN += c_hilite
c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(LIBSMBASE)
	./$@ >/dev/null


# ------------------ the editor ---------------------
EDITOR_OBJS := \
  buffer.o \
  buffercore.o \
  bufferstate.o \
  c_hilite.yy.o \
  comment.yy.o \
  editor.o \
  bufferlinesource.o \
  gotoline.o \
  history.o \
  historybuf.o \
  incsearch.o \
  inputproxy.o \
  lex_hilite.o \
  main.o \
  moc_editor.o \
  moc_gotoline.o \
  moc_main.o \
  pixmaps.o \
  status.o \
  style.o \
  styledb.o
-include $(EDITOR_OBJS:.o=.d)

TOCLEAN += editor
editor: $(EDITOR_OBJS) buffer.h textline.h position.h
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_OBJS) $(LDFLAGS)


# -------------- another test program ---------------
TOCLEAN += dialogs
dialogs: dialogs.cc
	$(CXX) -o $@ $(CCFLAGS) dialogs.cc -lXm -lXt $(LDFLAGS)


# --------------------- misc ------------------------
clean:
	rm -f $(TOCLEAN)

check:
	@echo "no useful 'make check' at this time"
