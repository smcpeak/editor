# Makefile for editor

#tmptarget: historybuf

# main target
all: testgap buffercore historybuf buffer style c_hilite editor


# directories of other software
SMBASE := smbase
LIBSMBASE := $(SMBASE)/libsmbase.a

# C++ compiler, etc.
CXX := g++

# flags for the C and C++ compilers (and preprocessor)
CCFLAGS := -g -Wall -I$(QTDIR)/include -I$(SMBASE)

# flags for the linker
LDFLAGS := -g -Wall $(LIBSMBASE) -L$(QTDIR)/lib -lqt


# patterns of files to delete in the 'clean' target; targets below
# add things to this using "+="
TOCLEAN =


# ---------------- pattern rules --------------------
# compile .cc to .o
TOCLEAN += *.o *.d
%.o : %.cc
	$(CXX) -c -o $@ $< $(CCFLAGS)
	@perl $(SMBASE)/depend.pl -o $@ $< $(CCFLAGS) > $*.d


# Qt meta-object compiler
moc_%.cc: %.h
	moc -o $@ $^


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
	mv $@ $*.tmp
	sed -e 's|class istream;|#include <iostream.h>|' \
	  <$*.tmp >$@
	rm $*.tmp

C_HILITE_OBJS := \
  buffercore.o \
  history.o \
  historybuf.o \
  buffer.o \
  style.o \
  lex_hilite.o \
  flexlexer.o \
  comment.yy.o \
  c_hilite.yy.o
#-include $(C_HILITE_OBJS:.o=.d)   # redundant with EDITOR_OBJS

TOCLEAN += c_hilite
c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(LIBSMBASE)
	./$@ >/dev/null


# ------------------ the editor ---------------------
EDITOR_OBJS := \
  incsearch.o \
  editor.o \
  moc_editor.o \
  buffercore.o \
  history.o \
  historybuf.o \
  buffer.o \
  main.o \
  moc_main.o \
  bufferstate.o \
  style.o \
  qtutil.o \
  styledb.o \
  c_hilite.yy.o \
  lex_hilite.o \
  flexlexer.o \
  comment.yy.o \
  inputproxy.o
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
