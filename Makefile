# Makefile for editor

# Default target.
all: comment.yy.cc
all: editor
all: test-prog-outs


# directories of other software
SMBASE   := ../smbase
SMFLEX   := ../smflex
SMQTUTIL := ../smqtutil

# Standard Makefile stuff.
include $(SMBASE)/sm-lib.mk

# C++ compiler, etc.
CXX := g++

# Set to 1 to activate coverage (gcov) support.
COVERAGE := 0

# Additional compile/link flags.
EXTRA_CCFLAGS :=
EXTRA_LDFLAGS :=

# Run a program with a timeout in case it hangs.
RUN_WITH_TIMEOUT := timeout 20

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
QT_CONSOLE_LDFLAGS += -L$(QT5LIB) -lQt5Core -Wl,-rpath=$(QT5LIB)
QT_CONSOLE_LDFLAGS += $(EXTRA_LDFLAGS)


# Create a target to run $1 and save the output to out/$1.out.
#
# The target is a .ok file instead of the .out itself because I do not
# want 'make' to delete the .out file if the command fails, since I want
# to be able to inspect it after the failure.
define RUN_TEST_PROG
TOCLEAN += $1
test-prog-outs: out/$1.ok
out/$1.ok: $1
	$$(CREATE_OUTPUT_DIRECTORY)
	$(RUN_WITH_TIMEOUT) ./$1 </dev/null >out/$1.out 2>&1
	touch $$@
endef


# Target to run the test programs.
.PHONY: test-prog-outs


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
EDITOR_OBJS += editor-strutil.o

TEST_EDITOR_STRUTIL_OBJS := $(EDITOR_OBJS)
TEST_EDITOR_STRUTIL_OBJS += test-editor-strutil.o
-include test-editor-strutil.d

test-editor-strutil: $(TEST_EDITOR_STRUTIL_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_EDITOR_STRUTIL_OBJS) $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-editor-strutil))


# ---------------- gap test program -----------------
testgap: gap.h testgap.cc
	$(CXX) -o $@ $(CCFLAGS) testgap.cc $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,testgap))


# -------------- test-td-core test program ----------------
TOCLEAN += td-core.tmp

EDITOR_OBJS += td-core.o
EDITOR_OBJS += textmcoord.o

TD_CORE_OBJS := $(EDITOR_OBJS)
TD_CORE_OBJS += test-td-core.o
-include test-td-core.d

test-td-core: $(TD_CORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_CORE_OBJS) $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-td-core))


# -------------- test-td-editor test program ----------------
TOCLEAN += td.tmp

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

$(eval $(call RUN_TEST_PROG,test-td-editor))


# -------------- test-text-search program ----------------
EDITOR_OBJS += fasttime.o
EDITOR_OBJS += text-search.o

TEST_TEXT_SEARCH_OBJS := $(EDITOR_OBJS)

TEST_TEXT_SEARCH_OBJS += test-text-search.o
-include test-text-search.d

test-text-search: $(TEST_TEXT_SEARCH_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_TEXT_SEARCH_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-text-search))


# -------------- justify test program ----------------
JUSTIFY_OBJS := $(EDITOR_OBJS)

JUSTIFY_OBJS += test-justify.o
-include test-justify.d

test-justify: $(JUSTIFY_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(JUSTIFY_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-justify))


# -------------- command-runner test program ----------------
EDITOR_OBJS += command-runner.o
EDITOR_OBJS += command-runner.moc.o

COMMAND_RUNNER_OBJS := $(EDITOR_OBJS)

COMMAND_RUNNER_OBJS += test-command-runner.o
COMMAND_RUNNER_OBJS += test-command-runner.moc.o
-include test-command-runner.d
-include test-command-runner.moc.d

test-command-runner: $(COMMAND_RUNNER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(COMMAND_RUNNER_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-command-runner))


# ---------------------- test-named-td ----------------------
EDITOR_OBJS += doc-name.o
EDITOR_OBJS += named-td.o

TEST_NAMED_TD_OBJS := $(EDITOR_OBJS)

TEST_NAMED_TD_OBJS += test-named-td.o
-include test-named-td.d

test-named-td: $(TEST_NAMED_TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_NAMED_TD_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-named-td))


# -------------------- test-named-td-list --------------------
EDITOR_OBJS += named-td-list.o

TEST_NAMED_TD_LIST_OBJS := $(EDITOR_OBJS)

TEST_NAMED_TD_LIST_OBJS += test-named-td-list.o
-include test-named-td-list.d

test-named-td-list: $(TEST_NAMED_TD_LIST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_NAMED_TD_LIST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-named-td-list))


# -------------------- test-nearby-file --------------------
EDITOR_OBJS += nearby-file.o

TEST_NEARBY_FILE_OBJS := $(EDITOR_OBJS)

TEST_NEARBY_FILE_OBJS += test-nearby-file.o
-include test-nearby-file.d

test-nearby-file: $(TEST_NEARBY_FILE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_NEARBY_FILE_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-nearby-file))


# --------------- textcategory test program ----------------
textcategory: textcategory.h textcategory.cc
	$(CXX) -o $@ $(CCFLAGS) -DTEST_TEXTCATEGORY textcategory.cc $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,textcategory))


# ----------------------- test-bufferlinesource ------------------------
EDITOR_OBJS += bufferlinesource.o

TEST_BUFFERLINESOURCE_OBJS := $(EDITOR_OBJS)

TEST_BUFFERLINESOURCE_OBJS += test-bufferlinesource.o
-include test-bufferlinesource.d

test-bufferlinesource: $(TEST_BUFFERLINESOURCE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_BUFFERLINESOURCE_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-bufferlinesource))


# ------------------------- editor-fs-server ---------------------------
EDITOR_FS_SERVER_OBJS :=
EDITOR_FS_SERVER_OBJS += editor-fs-server.o
EDITOR_FS_SERVER_OBJS += vfs-local.o
EDITOR_FS_SERVER_OBJS += vfs-msg.o

-include editor-fs-server.d
-include vfs-local.d
-include vfs-msg.d

editor-fs-server.exe: $(EDITOR_FS_SERVER_OBJS) $(LIBSMBASE)
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_FS_SERVER_OBJS) $(CONSOLE_LDFLAGS)

TOCLEAN += editor-fs-server.exe

all: editor-fs-server.exe


# ----------------------- editor-fs-server-test ------------------------
EDITOR_OBJS += host-name.o
EDITOR_OBJS += vfs-msg.o
EDITOR_OBJS += vfs-query.o
EDITOR_OBJS += vfs-query.moc.o
EDITOR_OBJS += waiting-counter.o

EDITOR_FS_SERVER_TEST_OBJS :=
EDITOR_FS_SERVER_TEST_OBJS += $(EDITOR_OBJS)
EDITOR_FS_SERVER_TEST_OBJS += editor-fs-server-test.o
EDITOR_FS_SERVER_TEST_OBJS += editor-fs-server-test.moc.o

-include editor-fs-server-test.d
-include editor-fs-server-test.moc.d

editor-fs-server-test.exe: $(EDITOR_FS_SERVER_TEST_OBJS) $(LIBSMBASE)
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_FS_SERVER_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,editor-fs-server-test.exe))

# The test uses the server executable.
out/editor-fs-server-test.exe.ok: editor-fs-server.exe


# ------------------------ vfs-connections-test ------------------------
EDITOR_OBJS += vfs-connections.o
EDITOR_OBJS += vfs-connections.moc.o

VFS_CONNECTIONS_TEST_OBJS :=
VFS_CONNECTIONS_TEST_OBJS += $(EDITOR_OBJS)
VFS_CONNECTIONS_TEST_OBJS += vfs-connections-test.o
VFS_CONNECTIONS_TEST_OBJS += vfs-connections-test.moc.o

-include vfs-connections-test.d
-include vfs-connections-test.mod.d

vfs-connections-test.exe: $(VFS_CONNECTIONS_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(VFS_CONNECTIONS_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,vfs-connections-test.exe))

# The test uses the server executable.
out/vfs-connections-test.exe.ok: editor-fs-server.exe


# Optionally run a test that uses SSH to connect to localhost.
ifeq ($(TEST_SSH_LOCALHOST),1)

test-prog-outs: out/vfs-connections-test.exe.localhost.ok
out/vfs-connections-test.exe.localhost.ok: vfs-connections-test.exe editor-fs-server.exe
	$(CREATE_OUTPUT_DIRECTORY)
	$(RUN_WITH_TIMEOUT) ./vfs-connections-test.exe localhost \
	  </dev/null >out/vfs-connections-test.exe.localhost.out 2>&1
	touch $@

endif # TEST_SSH_LOCALHOST


# ------------- highlighting stuff --------------------
TOCLEAN += *.yy.cc *.yy.h *.lex.backup

# lexer (-b makes lex.backup)
#
# TODO: The -b option is incompatible with parallel build, so I have
# removed it.  I should fix smflex and then re-enable this.
#
# NOTE: The base name of the lexer files can only use characters that
# are allowed in C identifiers because Flex derives the name of some
# internal identifiers from that file name.
%.yy.cc: %.lex %.h
	$(SMFLEX)/smflex -o$@ -P$*_yy $*.lex
	@#mv lex.backup $*.lex.backup
	@#cat $*.lex.backup

EDITOR_OBJS += c_hilite.yy.o
EDITOR_OBJS += comment.yy.o
EDITOR_OBJS += lex_hilite.o

C_HILITE_OBJS := $(EDITOR_OBJS)
-include c_hilite.d

c_hilite: $(C_HILITE_OBJS) c_hilite.cc
	$(CXX) -o $@ $(CCFLAGS) $(C_HILITE_OBJS) -DTEST_C_HILITE c_hilite.cc $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,c_hilite))


# ----------------- Makefile highlighting -----------------
EDITOR_OBJS += makefile_hilite.yy.o

TEST_MAKEFILE_HILITE_OBJS := $(EDITOR_OBJS)

TEST_MAKEFILE_HILITE_OBJS += test-makefile-hilite.o
-include test-makefile-hilite.d

test-makefile-hilite: $(TEST_MAKEFILE_HILITE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_MAKEFILE_HILITE_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-makefile-hilite))


# ----------------- HashComment highlighting -----------------
EDITOR_OBJS += hashcomment_hilite.yy.o

TEST_HASHCOMMENT_HILITE_OBJS := $(EDITOR_OBJS)

TEST_HASHCOMMENT_HILITE_OBJS += test-hashcomment-hilite.o
-include test-hashcomment-hilite.d

test-hashcomment-hilite: $(TEST_HASHCOMMENT_HILITE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEST_HASHCOMMENT_HILITE_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,test-hashcomment-hilite))


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

# The following list of object files is added to what has been set
# above.  Above, modules are pulled in as needed in order to build
# various test programs.  What's left over is the modules that are not
# part of any test program.
EDITOR_OBJS += connections-dialog.o
EDITOR_OBJS += connections-dialog.moc.o
EDITOR_OBJS += diff-hilite.o
EDITOR_OBJS += editor-global.o
EDITOR_OBJS += editor-global.moc.o
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
EDITOR_OBJS += vfs-query-sync.o
EDITOR_OBJS += vfs-query-sync.moc.o

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
	$(RM) -r out
	$(RM) *.gcov *.gcda *.gcno

check:
	./editor -ev=test/down.ev test/file1.h
	./editor -ev=test/copy-paste.ev test/file1.h
	./editor -ev=test/file-open-dialog1.ev test/file1.h
	./editor -ev=test/file-open-nonexist.ev
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
