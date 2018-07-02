# Makefile for editor

# main target
all: comment.yy.cc
all: testgap
all: td-core
all: test-td-editor
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

# Set to 1 to activate coverage (gcov) support.
COVERAGE := 0

# flags for the C and C++ compilers (and preprocessor)
CCFLAGS := -g -Wall -Wno-deprecated -std=c++11


# Pull in optional local configuration that can override the
# above variables.
-include config.mk


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

ifeq ($(COVERAGE),1)
  CCFLAGS += -fprofile-arcs -ftest-coverage
endif

# Flags for the linker for console programs.
CONSOLE_LDFLAGS := -g
CONSOLE_LDFLAGS += $(LIBSMBASE)

# Link flags for GUI programs.
GUI_LDFLAGS := $(CONSOLE_LDFLAGS)
GUI_LDFLAGS += $(LIBSMQTUTIL)
GUI_LDFLAGS += -L$(QTDIR_CPLR)/lib -lQt5Widgets -lQt5Gui -lQt5Core

# Link flags for console programs that use Qt5Core.  Specifically,
# I am using QRegularExpression in the 'justify' module.
QT_CONSOLE_LDFLAGS := $(CONSOLE_LDFLAGS)
QT_CONSOLE_LDFLAGS += $(LIBSMQTUTIL)
QT_CONSOLE_LDFLAGS += -L$(QTDIR_CPLR)/lib -lQt5Core

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
.PRECIOUS: moc-%.cc
TOCLEAN += moc-*.cc
moc-%.cc: %.h
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


# -------------- td-core test program ----------------
TOCLEAN += td-core td-core.tmp
TD_CORE_OBJS := td-core.cc textcoord.o
td-core: gap.h $(TD_CORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_TD_CORE $(TD_CORE_OBJS) $(CONSOLE_LDFLAGS)
	./td-core >/dev/null 2>&1

# -------------- test-td-editor test program ----------------
TOCLEAN += test-td-editor td.tmp

TD_OBJS :=
TD_OBJS += history.o
TD_OBJS += justify.o
TD_OBJS += test-td-editor.o
TD_OBJS += td.o
TD_OBJS += td-core.o
TD_OBJS += td-editor.o
TD_OBJS += textcoord.o

test-td-editor: $(TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-td-editor >/dev/null 2>&1

-include test-td-editor.d

# -------------- justify test program ----------------
TOCLEAN += test-justify

JUSTIFY_OBJS :=
JUSTIFY_OBJS += history.o
JUSTIFY_OBJS += justify.o
JUSTIFY_OBJS += td.o
JUSTIFY_OBJS += td-core.o
JUSTIFY_OBJS += td-editor.o
JUSTIFY_OBJS += test-justify.o
JUSTIFY_OBJS += textcoord.o

test-justify: $(JUSTIFY_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(JUSTIFY_OBJS) $(QT_CONSOLE_LDFLAGS)
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

C_HILITE_OBJS :=
C_HILITE_OBJS += bufferlinesource.o
C_HILITE_OBJS += c_hilite.yy.o
C_HILITE_OBJS += comment.yy.o
C_HILITE_OBJS += history.o
C_HILITE_OBJS += justify.o
C_HILITE_OBJS += lex_hilite.o
C_HILITE_OBJS += td.o
C_HILITE_OBJS += td-core.o
C_HILITE_OBJS += td-editor.o
C_HILITE_OBJS += textcategory.o
C_HILITE_OBJS += textcoord.o

#-include $(C_HILITE_OBJS:.o=.d)   # redundant with EDITOR_OBJS

TOCLEAN += c_hilite
c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(QT_CONSOLE_LDFLAGS)
	./$@ >/dev/null


# ------------------ the editor ---------------------
# This file needs a generated file to exist.
editor-window.o: keybindings.doc.gen.h

TOCLEAN += keybindings.doc.gen.*

EDITOR_OBJS :=
EDITOR_OBJS += bufferlinesource.o
EDITOR_OBJS += c_hilite.yy.o
EDITOR_OBJS += comment.yy.o
EDITOR_OBJS += editor-widget.o
EDITOR_OBJS += editor-window.o
EDITOR_OBJS += file-td.o
EDITOR_OBJS += history.o
EDITOR_OBJS += incsearch.o
EDITOR_OBJS += inputproxy.o
EDITOR_OBJS += justify.o
EDITOR_OBJS += keybindings.doc.gen.o
EDITOR_OBJS += lex_hilite.o
EDITOR_OBJS += main.o
EDITOR_OBJS += moc-editor-widget.o
EDITOR_OBJS += moc-editor-window.o
EDITOR_OBJS += moc-main.o
EDITOR_OBJS += moc-textinput.o
EDITOR_OBJS += pixmaps.o
EDITOR_OBJS += status.o
EDITOR_OBJS += styledb.o
EDITOR_OBJS += td.o
EDITOR_OBJS += td-core.o
EDITOR_OBJS += td-editor.o
EDITOR_OBJS += textcategory.o
EDITOR_OBJS += textcoord.o
EDITOR_OBJS += textinput.o

-include $(EDITOR_OBJS:.o=.d)

TOCLEAN += editor
editor: $(EDITOR_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_OBJS) $(GUI_LDFLAGS)


# -------------- another test program ---------------
TOCLEAN += dialogs
dialogs: dialogs.cc
	$(CXX) -o $@ $(CCFLAGS) dialogs.cc -lXm -lXt $(GUI_LDFLAGS)


# --------------------- misc ------------------------
clean:
	$(RM) $(TOCLEAN)
	$(RM) *.gcov *.gcda *.gcno

check:
	@echo "no useful 'make check' at this time"

# EOF
