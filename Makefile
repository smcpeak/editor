# Makefile for editor

# main target
all: comment.yy.cc
all: testgap
all: text-document-core
all: text-document
all: buffer
all: test-justify
all: textcategory
all: c_hilite
all: editor


# directories of other software
SMBASE := ../smbase
LIBSMBASE := $(SMBASE)/libsmbase.a

SMQTUTIL := ../smqtutil
LIBSMQTUTIL := $(SMQTUTIL)/libsmqtutil.a

# Directory with Qt headers, libraries, and programs.
QTDIR := /d/bld/qt5/qtbase

# Same, as a path that can be passed to the compiler.
QTDIR_CPLR := $(shell cygpath -m '$(QTDIR)')


# C++ compiler, etc.
CXX := x86_64-w64-mingw32-g++

# flags for the C and C++ compilers (and preprocessor)
CCFLAGS := -g -Wall -Wno-deprecated -std=c++11

# The "-I." is so that "include <FlexLexer.h>" will pull in the
# FlexLexer.h in the currect directory, which is a copy of the
# one that Cygwin put in /usr/include.  I cannot directly use that
# one because the mingw compiler does not look in /usr/include
# since that has the native cygwin library headers.
CCFLAGS += -I.

CCFLAGS += -I$(SMBASE)
CCFLAGS += -I$(SMQTUTIL)

CCFLAGS += -I$(QTDIR_CPLR)/include
CCFLAGS += -I$(QTDIR_CPLR)/include/QtCore
CCFLAGS += -I$(QTDIR_CPLR)/include/QtGui
CCFLAGS += -I$(QTDIR_CPLR)/include/QtWidgets

# Flags for the linker for console programs.
CONSOLE_LDFLAGS := -g
CONSOLE_LDFLAGS += $(LIBSMBASE)

# Link flags for GUI programs.
GUI_LDFLAGS := $(CONSOLE_LDFLAGS)
GUI_LDFLAGS += $(LIBSMQTUTIL)
GUI_LDFLAGS += -L$(QTDIR_CPLR)/lib -lQt5Widgets -lQt5Gui -lQt5Core

# Qt build tools
MOC := $(QTDIR)/bin/moc
UIC := $(QTDIR)/bin/uic


# patterns of files to delete in the 'clean' target; targets below
# add things to this using "+="
TOCLEAN =


# ---------------- pattern rules --------------------
# compile .cc to .o
# -MMD causes GCC to write .d file.
# The -MP modifier adds phony targets to deal with removed headers.
TOCLEAN += *.o *.d
%.o : %.cc
	$(CXX) -c -MMD -MP -o $@ $< $(CCFLAGS)


# Qt meta-object compiler
.PRECIOUS: moc_%.cc
TOCLEAN += moc_*.cc
moc_%.cc: %.h
	$(MOC) -o $@ $^


# Qt designer translator
%.gen.h: %.ui
	$(UIC) -o $*.gen.h $*.ui


# Encode help files as C string literals.
%.doc.gen.cc %.doc.gen.h: doc/%.txt
	perl $(SMBASE)/file-to-strlit.pl doc_$* $^ $*.doc.gen.h $*.doc.gen.cc


# ---------------- gap test program -----------------
TOCLEAN += testgap
testgap: gap.h testgap.cc
	$(CXX) -o $@ $(CCFLAGS) testgap.cc $(CONSOLE_LDFLAGS)
	./testgap >/dev/null 2>&1


# -------------- text-document-core test program ----------------
TOCLEAN += text-document-core text-document-core.tmp
TEXT_DOCUMENT_CORE_OBJS := text-document-core.cc textcoord.o
text-document-core: gap.h $(TEXT_DOCUMENT_CORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_TEXT_DOCUMENT_CORE $(TEXT_DOCUMENT_CORE_OBJS) $(CONSOLE_LDFLAGS)
	./text-document-core >/dev/null 2>&1

# -------------- text-document test program ----------------
TOCLEAN += text-document text-document.tmp
TEXT_DOCUMENT_OBJS := text-document.cc history.o text-document-core.o textcoord.o
text-document: gap.h $(TEXT_DOCUMENT_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_TEXT_DOCUMENT $(TEXT_DOCUMENT_OBJS) $(CONSOLE_LDFLAGS)
	./text-document >/dev/null 2>&1

# -------------- buffer test program ----------------
TOCLEAN += buffer
BUFFER_OBJS := buffer.cc text-document-core.o textcoord.o history.o text-document.o
buffer: gap.h $(BUFFER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_BUFFER $(BUFFER_OBJS) $(CONSOLE_LDFLAGS)
	./buffer >/dev/null 2>&1

# -------------- justify test program ----------------
TOCLEAN += test-justify
JUSTIFY_OBJS := test-justify.o justify.o buffer.o text-document-core.o textcoord.o history.o text-document.o
test-justify: $(JUSTIFY_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(JUSTIFY_OBJS) $(GUI_LDFLAGS)
	./test-justify >/dev/null 2>&1

-include test-justify.d

# --------------- textcategory test program ----------------
TOCLEAN += textcategory
textcategory: textcategory.h textcategory.cc
	$(CXX) -o $@ $(CCFLAGS) -DTEST_TEXTCATEGORY textcategory.cc $(CONSOLE_LDFLAGS)
	./textcategory >/dev/null


# ------------- highlighting stuff --------------------
# lexer (-b makes lex.backup)
TOCLEAN += comment.yy.cc c_hilite.yy.cc *.lex.backup
%.yy.cc: %.lex %.h
	flex -o$@ -b -P$*_yy $*.lex
	mv lex.backup $*.lex.backup
	head $*.lex.backup

C_HILITE_OBJS := \
  text-document-core.o \
  textcoord.o \
  history.o \
  text-document.o \
  buffer.o \
  textcategory.o \
  lex_hilite.o \
  bufferlinesource.o \
  comment.yy.o \
  c_hilite.yy.o
#-include $(C_HILITE_OBJS:.o=.d)   # redundant with EDITOR_OBJS

TOCLEAN += c_hilite
c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(CONSOLE_LDFLAGS)
	./$@ >/dev/null


# ------------------ the editor ---------------------
main.o: gotoline.gen.h
main.o: keybindings.doc.gen.h

EDITOR_OBJS := \
  buffer.o \
  text-document-file.o \
  c_hilite.yy.o \
  comment.yy.o \
  editor.o \
  bufferlinesource.o \
  history.o \
  text-document.o \
  incsearch.o \
  inputproxy.o \
  justify.o \
  keybindings.doc.gen.o \
  lex_hilite.o \
  main.o \
  moc_editor.o \
  moc_main.o \
  pixmaps.o \
  status.o \
  textcategory.o \
  styledb.o \
  text-document-core.o \
  textcoord.o
-include $(EDITOR_OBJS:.o=.d)

TOCLEAN += editor
editor: $(EDITOR_OBJS) buffer.h textline.h position.h
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_OBJS) $(GUI_LDFLAGS)


# -------------- another test program ---------------
TOCLEAN += dialogs
dialogs: dialogs.cc
	$(CXX) -o $@ $(CCFLAGS) dialogs.cc -lXm -lXt $(GUI_LDFLAGS)


# --------------------- misc ------------------------
clean:
	$(RM) $(TOCLEAN)

check:
	@echo "no useful 'make check' at this time"

# EOF
