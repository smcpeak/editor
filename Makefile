# Makefile for editor

# Default target.
all: comment.yy.cc
all: testgap
all: test-td-core
all: test-td-editor
all: test-text-search
all: test-justify
all: test-command-runner
all: test-named-td
all: test-named-td-list
all: test-nearby-file
all: textcategory
all: test-bufferlinesource
all: c_hilite
all: test-makefile-hilite
all: test-hashcomment-hilite
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


# Redirections for test programs.  I do not want to pollute the 'make'
# output with their diagnostics, but I also do not want to lose the
# output in case it fails.
TEST_REDIR := >test.out 2>&1


# patterns of files to delete in the 'clean' target; targets below
# add things to this using "+="
TOCLEAN = $(QT_TOCLEAN)


# Object files for the editor.  This is built up gradually as needed
# for various test programs, roughly in bottom-up order.
EDITOR_OBJS :=


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


# ------------ test-editor-strutil program -------------
TOCLEAN += test-editor-strutil

EDITOR_OBJS += editor-strutil.o

TEST_EDITOR_STRUTIL_OBJS := $(EDITOR_OBJS)
TEST_EDITOR_STRUTIL_OBJS += test-editor-strutil.o
-include test-editor-strutil.d

test-editor-strutil: $(TEST_EDITOR_STRUTIL_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_EDITOR_STRUTIL_OBJS) $(CONSOLE_LDFLAGS)
	./test-editor-strutil $(TEST_REDIR)


# ---------------- gap test program -----------------
TOCLEAN += testgap
testgap: gap.h testgap.cc
	$(CXX) -o $@ $(CCFLAGS) testgap.cc $(CONSOLE_LDFLAGS)
	./testgap $(TEST_REDIR)


# -------------- test-td-core test program ----------------
TOCLEAN += test-td-core td-core.tmp

EDITOR_OBJS += td-core.o
EDITOR_OBJS += textmcoord.o

TD_CORE_OBJS := $(EDITOR_OBJS)
TD_CORE_OBJS += test-td-core.o

test-td-core: $(TD_CORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_CORE_OBJS) $(CONSOLE_LDFLAGS)
	./test-td-core $(TEST_REDIR)

-include test-td-core.d


# -------------- test-td-editor test program ----------------
TOCLEAN += test-td-editor td.tmp

EDITOR_OBJS += history.o
EDITOR_OBJS += justify.o
EDITOR_OBJS += td.o
EDITOR_OBJS += td-editor.o
EDITOR_OBJS += textcategory.o
EDITOR_OBJS += textlcoord.o

TD_OBJS := $(EDITOR_OBJS)

TD_OBJS += test-td-editor.o
-include test-td-editor.d

test-td-editor: $(TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-td-editor $(TEST_REDIR)


# -------------- test-text-search program ----------------
TOCLEAN += test-text-search

EDITOR_OBJS += fasttime.o
EDITOR_OBJS += text-search.o

TEST_TEXT_SEARCH_OBJS := $(EDITOR_OBJS)

TEST_TEXT_SEARCH_OBJS += test-text-search.o
-include test-text-search.d

test-text-search: $(TEST_TEXT_SEARCH_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_TEXT_SEARCH_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-text-search $(TEST_REDIR)


# -------------- justify test program ----------------
TOCLEAN += test-justify

JUSTIFY_OBJS := $(EDITOR_OBJS)

JUSTIFY_OBJS += test-justify.o
-include test-justify.d

test-justify: $(JUSTIFY_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(JUSTIFY_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-justify $(TEST_REDIR)


# -------------- command-runner test program ----------------
TOCLEAN += test-command-runner

EDITOR_OBJS += command-runner.o
EDITOR_OBJS += command-runner.moc.o

COMMAND_RUNNER_OBJS := $(EDITOR_OBJS)

COMMAND_RUNNER_OBJS += test-command-runner.o
COMMAND_RUNNER_OBJS += test-command-runner.moc.o
-include test-command-runner.d
-include test-command-runner.moc.d

test-command-runner: $(COMMAND_RUNNER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(COMMAND_RUNNER_OBJS) $(QT_CONSOLE_LDFLAGS)
	timeout 10 ./test-command-runner $(TEST_REDIR)

# In the above command, 'timeout' is used because the command runner
# is particularly susceptible to bugs that cause hangs.  Plus, when I
# hit Ctrl+C to kill it, 'make' deletes the executable (!), which is
# annoying.


# ---------------------- test-named-td ----------------------
TOCLEAN += test-named-td

EDITOR_OBJS += named-td.o

TEST_NAMED_TD_OBJS := $(EDITOR_OBJS)

TEST_NAMED_TD_OBJS += test-named-td.o
-include test-named-td.d

test-named-td: $(TEST_NAMED_TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_NAMED_TD_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-named-td $(TEST_REDIR)


# -------------------- test-named-td-list --------------------
TOCLEAN += test-named-td-list

EDITOR_OBJS += named-td-list.o

TEST_NAMED_TD_LIST_OBJS := $(EDITOR_OBJS)

TEST_NAMED_TD_LIST_OBJS += test-named-td-list.o
-include test-named-td-list.d

test-named-td-list: $(TEST_NAMED_TD_LIST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_NAMED_TD_LIST_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-named-td-list $(TEST_REDIR)


# -------------------- test-nearby-file --------------------
TOCLEAN += test-nearby-file

EDITOR_OBJS += nearby-file.o

TEST_NEARBY_FILE_OBJS := $(EDITOR_OBJS)

TEST_NEARBY_FILE_OBJS += test-nearby-file.o
-include test-nearby-file.d

test-nearby-file: $(TEST_NEARBY_FILE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_NEARBY_FILE_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-nearby-file $(TEST_REDIR)


# --------------- textcategory test program ----------------
TOCLEAN += textcategory
textcategory: textcategory.h textcategory.cc
	$(CXX) -o $@ $(CCFLAGS) -DTEST_TEXTCATEGORY textcategory.cc $(CONSOLE_LDFLAGS)
	./textcategory >/dev/null


# ----------------------- test-bufferlinesource ------------------------
TOCLEAN += test-bufferlinesource

EDITOR_OBJS += bufferlinesource.o

TEST_BUFFERLINESOURCE_OBJS := $(EDITOR_OBJS)

TEST_BUFFERLINESOURCE_OBJS += test-bufferlinesource.o
-include test-bufferlinesource.d

test-bufferlinesource: $(TEST_BUFFERLINESOURCE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_BUFFERLINESOURCE_OBJS) $(QT_CONSOLE_LDFLAGS)
	./test-bufferlinesource $(TEST_REDIR)


# ------------- highlighting stuff --------------------
TOCLEAN += *.yy.cc *.lex.backup

# lexer (-b makes lex.backup)
#
# NOTE: The base name of the lexer files can only use characters that
# are allowed in C identifiers because Flex derives the name of some
# internal identifiers from that file name.
%.yy.cc: %.lex %.h
	flex -o$@ -b -P$*_yy $*.lex
	mv lex.backup $*.lex.backup
	cat $*.lex.backup

EDITOR_OBJS += c_hilite.yy.o
EDITOR_OBJS += comment.yy.o
EDITOR_OBJS += lex_hilite.o

C_HILITE_OBJS := $(EDITOR_OBJS)

TOCLEAN += c_hilite
c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(QT_CONSOLE_LDFLAGS)
	./$@ $(TEST_REDIR)

-include c_hilite.d


# ----------------- Makefile highlighting -----------------
EDITOR_OBJS += makefile_hilite.yy.o

TEST_MAKEFILE_HILITE_OBJS := $(EDITOR_OBJS)

TEST_MAKEFILE_HILITE_OBJS += test-makefile-hilite.o
-include test-makefile-hilite.d

TOCLEAN += test-makefile-hilite
test-makefile-hilite: $(TEST_MAKEFILE_HILITE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_MAKEFILE_HILITE_OBJS) $(QT_CONSOLE_LDFLAGS)
	./$@ $(TEST_REDIR)


# ----------------- HashComment highlighting -----------------
EDITOR_OBJS += hashcomment_hilite.yy.o

TEST_HASHCOMMENT_HILITE_OBJS := $(EDITOR_OBJS)

TEST_HASHCOMMENT_HILITE_OBJS += test-hashcomment-hilite.o
-include test-hashcomment-hilite.d

TOCLEAN += test-hashcomment-hilite
test-hashcomment-hilite: $(TEST_HASHCOMMENT_HILITE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_HASHCOMMENT_HILITE_OBJS) $(QT_CONSOLE_LDFLAGS)
	./$@ $(TEST_REDIR)


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

TOCLEAN += git-version.gen.*

# ------------------ the editor ---------------------
# editor-window.cc includes keybindings.doc.gen.h.
editor-window.o: keybindings.doc.gen.h

TOCLEAN += keybindings.doc.gen.*

EDITOR_OBJS += diff-hilite.o
EDITOR_OBJS += editor-widget.o
EDITOR_OBJS += editor-widget.moc.o
EDITOR_OBJS += editor-window.o
EDITOR_OBJS += editor-window.moc.o
EDITOR_OBJS += event-recorder.o
EDITOR_OBJS += event-replay.o
EDITOR_OBJS += event-replay.moc.o
EDITOR_OBJS += filename-input.o
EDITOR_OBJS += filename-input.moc.o
EDITOR_OBJS += git-version.gen.o
EDITOR_OBJS += keybindings.doc.gen.o
EDITOR_OBJS += keys-dialog.o
EDITOR_OBJS += launch-command-dialog.o
EDITOR_OBJS += main.o
EDITOR_OBJS += main.moc.o
EDITOR_OBJS += modal-dialog.o
EDITOR_OBJS += modal-dialog.moc.o
EDITOR_OBJS += my-table-widget.o
EDITOR_OBJS += open-files-dialog.o
EDITOR_OBJS += open-files-dialog.moc.o
EDITOR_OBJS += pixmaps.o
EDITOR_OBJS += process-watcher.o
EDITOR_OBJS += process-watcher.moc.o
EDITOR_OBJS += resources.qrc.gen.o
EDITOR_OBJS += sar-panel.o
EDITOR_OBJS += sar-panel.moc.o
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
	./editor -ev=test/down.ev test/file1.h
	./editor -ev=test/copy-paste.ev test/file1.h
	./editor -ev=test/file-open-dialog1.ev test/file1.h
	./editor -ev=test/simple-text-ins.ev
	./editor -ev=test/sar-find-cs-completion.ev test/file1.h
	./editor -ev=test/sar-repl-cs-completion.ev test/file1.h
	./editor -ev=test/sar-match-limit.ev test/file1.h
	./editor -ev=test/sar-replace-bol.ev test/file1.h
	./editor -ev=test/resize1.ev test/file1.h
	./editor -ev=test/read-only1.ev
	./editor -ev=test/read-only2.ev
	./editor -ev=test/read-only3.ev
	./editor -ev=test/prompt-unsaved-changes.ev
	./editor -ev=test/screenshot1.ev test/file1.h
	./editor -ev=test/visible-tabs.ev test/tabs-test.txt
	./editor -ev=test/search-hits-with-tab.ev test/tabs-test.txt
	cp test/file1.h tmp.h && ./editor -ev=test/undo-redo-undo.ev tmp.h && rm tmp.h
	./editor -ev=test/keysequence.ev
	./editor -ev=test/keysequence2.ev
	cd test && ../editor -ev=fn-input-dialog-size.ev
	./editor -ev=test/screenshot-has-tabs.ev test/has-tabs.c
	./editor -ev=test/open-files-close-docs.ev test/read-only1.ev test/read-only2.ev test/read-only3.ev test/resize1.ev
	./editor -ev=test/cut-then-paste.ev
	./editor -ev=test/select-beyond-eof.ev
	cp test/robotank.info.json.crlf.bin tmp.h && \
	  ./editor -ev=test/sar-remove-ctlm.ev tmp.h && \
	  cmp tmp.h test/robotank.info.json.lf.bin && \
	  rm tmp.h
	./editor -ev=test/fn-input-repeat-tab.ev

# EOF
