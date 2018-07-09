# Makefile for editor

# main target
all: comment.yy.cc
all: testgap
all: test-td-core
all: test-td-editor
all: test-justify
all: test-command-runner
all: test-file-td
all: test-file-td-list
all: textcategory
all: c_hilite
all: editor


# directories of other software
SMBASE := ../smbase
SMQTUTIL := ../smqtutil

# C++ compiler, etc.
CXX := g++

# Set to 1 to activate coverage (gcov) support.
COVERAGE := 0

# Additional compile/link flags.
EXTRA_CCFLAGS :=
EXTRA_LDFLAGS :=

# Pull in build configuration.  This must provide definitions of
# QT5INCLUDE, QT5LIB and QT5BIN.  It can optionally override the
# variables defined above.
ifeq (,$(wildcard config.mk))
$(error The file config.mk does not exist.  You have to copy config.mk.template to config.mk and then edit the latter by hand)
endif
include config.mk

# Set QT_CCFLAGS, QT_LDFLAGS, QT_TOCLEAN, and the 'moc' rule.
include $(SMQTUTIL)/qtvars.mk


# Flags for the C and C++ compilers (and preprocessor).
CCFLAGS := -g -Wall -Wno-deprecated -std=c++11

# The "-I." is so that "include <FlexLexer.h>" will pull in the
# FlexLexer.h in the currect directory, which is a copy of the
# one that Cygwin put in /usr/include.  I cannot directly use that
# one because the mingw compiler does not look in /usr/include
# since that has the native cygwin library headers.
CCFLAGS += -I.

CCFLAGS += -I$(SMBASE)
CCFLAGS += -I$(SMQTUTIL)
CCFLAGS += $(QT_CCFLAGS)
CCFLAGS += $(EXTRA_CCFLAGS)

ifeq ($(COVERAGE),1)
  CCFLAGS += -fprofile-arcs -ftest-coverage
endif

LIBSMBASE := $(SMBASE)/libsmbase.a
LIBSMQTUTIL := $(SMQTUTIL)/libsmqtutil.a

# Flags for the linker for console programs.
CONSOLE_LDFLAGS := -g
CONSOLE_LDFLAGS += $(LIBSMBASE)
CONSOLE_LDFLAGS += $(EXTRA_LDFLAGS)

# Link flags for GUI programs.
GUI_LDFLAGS := -g
GUI_LDFLAGS += $(LIBSMQTUTIL)
GUI_LDFLAGS += $(LIBSMBASE)
GUI_LDFLAGS += $(QT_LDFLAGS)
GUI_LDFLAGS += $(EXTRA_LDFLAGS)

# Link flags for console programs that use Qt5Core.  Specifically,
# I am using QRegularExpression in the 'justify' module.
QT_CONSOLE_LDFLAGS := -g
QT_CONSOLE_LDFLAGS += $(LIBSMQTUTIL)
QT_CONSOLE_LDFLAGS += $(LIBSMBASE)
QT_CONSOLE_LDFLAGS += -L$(QT5LIB) -lQt5Core
QT_CONSOLE_LDFLAGS += $(EXTRA_LDFLAGS)


# patterns of files to delete in the 'clean' target; targets below
# add things to this using "+="
TOCLEAN = $(QT_TOCLEAN)


# ---------------- pattern rules --------------------
# compile .cc to .o
# -MMD causes GCC to write .d file.
# The -MP modifier adds phony targets to deal with removed headers.
TOCLEAN += *.o *.d
%.o : %.cc
	$(CXX) -c -MMD -MP -o $@ $< $(CCFLAGS)


# Encode help files as C string literals.
%.doc.gen.cc %.doc.gen.h: doc/%.txt
	perl $(SMBASE)/file-to-strlit.pl doc_$* $^ $*.doc.gen.h $*.doc.gen.cc


# ---------------- gap test program -----------------
TOCLEAN += testgap
testgap: gap.h testgap.cc
	$(CXX) -o $@ $(CCFLAGS) testgap.cc $(CONSOLE_LDFLAGS)
	./testgap >/dev/null 2>&1


# -------------- test-td-core test program ----------------
TOCLEAN += test-td-core td-core.tmp
TD_CORE_OBJS := test-td-core.o td-core.o textcoord.o
test-td-core: $(TD_CORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_CORE_OBJS) $(CONSOLE_LDFLAGS)
	./test-td-core >/dev/null 2>&1

-include test-td-core.d


# -------------- test-td-editor test program ----------------
TOCLEAN += test-td-editor td.tmp

# Object files for the editor.  This is built up gradually as needed
# for various test programs, roughly in bottom-up order.
EDITOR_OBJS :=
EDITOR_OBJS += history.o
EDITOR_OBJS += justify.o
EDITOR_OBJS += td.o
EDITOR_OBJS += td-core.o
EDITOR_OBJS += td-editor.o
EDITOR_OBJS += textcoord.o

TD_OBJS := $(EDITOR_OBJS)

TD_OBJS += test-td-editor.o
-include test-td-editor.d

test-td-editor: $(TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-td-editor >/dev/null 2>&1


# -------------- justify test program ----------------
TOCLEAN += test-justify

JUSTIFY_OBJS := $(EDITOR_OBJS)

JUSTIFY_OBJS += test-justify.o
-include test-justify.d

test-justify: $(JUSTIFY_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(JUSTIFY_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-justify >/dev/null 2>&1


# -------------- command-runner test program ----------------
TOCLEAN += test-command-runner

EDITOR_OBJS += command-runner.o
EDITOR_OBJS += command-runner.moc.o

COMMAND_RUNNER_OBJS := $(EDITOR_OBJS)

COMMAND_RUNNER_OBJS += test-command-runner.o
-include test-command-runner.d

test-command-runner: $(COMMAND_RUNNER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(COMMAND_RUNNER_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-command-runner >/dev/null 2>&1


# ---------------------- test-file-td ----------------------
TOCLEAN += test-file-td

EDITOR_OBJS += file-td.o

TEST_FILE_TD_OBJS := $(EDITOR_OBJS)

TEST_FILE_TD_OBJS += test-file-td.o
-include test-file-td.d

test-file-td: $(TEST_FILE_TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_FILE_TD_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-file-td >/dev/null 2>&1


# -------------------- test-file-td-list --------------------
TOCLEAN += test-file-td-list

EDITOR_OBJS += file-td-list.o

TEST_FILE_TD_LIST_OBJS := $(EDITOR_OBJS)

TEST_FILE_TD_LIST_OBJS += test-file-td-list.o
-include test-file-td-list.d

test-file-td-list: $(TEST_FILE_TD_LIST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_FILE_TD_LIST_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-file-td-list >/dev/null 2>&1


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

EDITOR_OBJS += bufferlinesource.o
EDITOR_OBJS += c_hilite.yy.o
EDITOR_OBJS += comment.yy.o
EDITOR_OBJS += lex_hilite.o
EDITOR_OBJS += textcategory.o

C_HILITE_OBJS := $(EDITOR_OBJS)

TOCLEAN += c_hilite
c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(QT_CONSOLE_LDFLAGS)
	./$@ >/dev/null

-include c_hilite.d


# ----------------- git version ---------------------
# Command to query git for the version.  Output is like:
# "d36671f 2018-07-09 04:10:32 -0700".
GIT_VERSION_QUERY := git log -n 1 --format=format:'%h %ai%n'

ifeq (,$(wildcard .git/HEAD))

# Source code is not in a git repository.
git-version.gen.txt:
	echo "No git version info available." > $@

else

GIT_HEAD_CONTENTS := $(shell cat .git/HEAD)
#$(info GIT_HEAD_CONTENTS: $(GIT_HEAD_CONTENTS))

# Is HEAD a reference to a branch?
ifeq (,$(findstring ref: ,$(GIT_HEAD_CONTENTS)))

# HEAD is detached.
git-version.gen.txt: .git/HEAD
	$(GIT_VERSION_QUERY) > $@

else

# HEAD is on branch.
GIT_BRANCH_REF := $(filter refs/%,$(GIT_HEAD_CONTENTS))
#$(info GIT_BRANCH_REF: $(GIT_BRANCH_REF))

git-version.gen.txt: .git/HEAD .git/$(GIT_BRANCH_REF)
	$(GIT_VERSION_QUERY) > $@

endif

endif

# I do not use the generated header file but the generated .cc wants it.
#
# TODO: I should change file-to-strlit.pl so it can avoid making that.
git-version.gen.cc: git-version.gen.txt
	perl $(SMBASE)/file-to-strlit.pl editor_git_version $^ git-version.gen.h $@

# ------------------ the editor ---------------------
# editor-window.cc includes keybindings.doc.gen.h.
editor-window.o: keybindings.doc.gen.h

TOCLEAN += keybindings.doc.gen.*

EDITOR_OBJS += editor-widget.o
EDITOR_OBJS += editor-widget.moc.o
EDITOR_OBJS += editor-window.o
EDITOR_OBJS += editor-window.moc.o
EDITOR_OBJS += ftdl-table-model.o
EDITOR_OBJS += git-version.gen.o
EDITOR_OBJS += incsearch.o
EDITOR_OBJS += inputproxy.o
EDITOR_OBJS += keybindings.doc.gen.o
EDITOR_OBJS += keys-dialog.o
EDITOR_OBJS += main.o
EDITOR_OBJS += main.moc.o
EDITOR_OBJS += modal-dialog.o
EDITOR_OBJS += modal-dialog.moc.o
EDITOR_OBJS += my-table-view.o
EDITOR_OBJS += open-files-dialog.o
EDITOR_OBJS += open-files-dialog.moc.o
EDITOR_OBJS += pixmaps.o
EDITOR_OBJS += status.o
EDITOR_OBJS += styledb.o
EDITOR_OBJS += textinput.o
EDITOR_OBJS += textinput.moc.o

-include $(EDITOR_OBJS:.o=.d)

TOCLEAN += editor
editor: $(EDITOR_OBJS) $(LIBSMQTUTIL) $(LIBSMBASE)
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_OBJS) $(GUI_LDFLAGS)


# -------------- another test program ---------------
# This is an old test program written on top of bare X.
# TODO: I should remove it.
TOCLEAN += dialogs
dialogs: dialogs.cc
	$(CXX) -o $@ $(CCFLAGS) dialogs.cc -lXm -lXt $(GUI_LDFLAGS)


# --------------------- misc ------------------------
clean:
	$(RM) $(TOCLEAN)
	$(RM) *.gcov *.gcda *.gcno

check:
	@echo "There is no useful 'make check' at this time;"
	@echo "plain 'make' runs all the automated tests."

# EOF
