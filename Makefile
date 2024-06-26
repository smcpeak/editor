# Makefile for editor

# Default target.
all: comment.yy.cc
all: editor
all: test-prog-outs


# directories of other software
SMBASE   := smbase
SMFLEX   := smflex
SMQTUTIL := smqtutil

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
#
# C++17 is needed for `std::string_view`, used by smbase.
CCFLAGS := -g -Wall -std=c++17

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


# Command prefix to retry a test up to 3 times.
RETRY := sh ./retry.sh 3


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


# ------------ editor-strutil-test program -------------
EDITOR_OBJS += editor-strutil.o

EDITOR_STRUTIL_TEST_OBJS := $(EDITOR_OBJS)
EDITOR_STRUTIL_TEST_OBJS += editor-strutil-test.o
-include editor-strutil-test.d

editor-strutil-test: $(EDITOR_STRUTIL_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_STRUTIL_TEST_OBJS) $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,editor-strutil-test))


# ----------------------------- gap-test -------------------------------
gap-test: gap.h gap-test.cc
	$(CXX) -o $@ $(CCFLAGS) gap-test.cc $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,gap-test))


# -------------- td-core-test test program ----------------
TOCLEAN += td-core.tmp

EDITOR_OBJS += td-core.o
EDITOR_OBJS += textmcoord.o

TD_CORE_OBJS := $(EDITOR_OBJS)
TD_CORE_OBJS += td-core-test.o
-include td-core-test.d

td-core-test: $(TD_CORE_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_CORE_OBJS) $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,td-core-test))


# -------------- td-editor-test test program ----------------
TOCLEAN += td.tmp

EDITOR_OBJS += history.o
EDITOR_OBJS += justify.o
EDITOR_OBJS += td.o
EDITOR_OBJS += td-editor.o
EDITOR_OBJS += textcategory.o
EDITOR_OBJS += textlcoord.o

TD_OBJS := $(EDITOR_OBJS)

TD_OBJS += td-editor-test.o
-include td-editor-test.d

td-editor-test: $(TD_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TD_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,td-editor-test))


# -------------- text-search-test program ----------------
EDITOR_OBJS += fasttime.o
EDITOR_OBJS += text-search.o

TEXT_SEARCH_TEST_OBJS := $(EDITOR_OBJS)

TEXT_SEARCH_TEST_OBJS += text-search-test.o
-include text-search-test.d

text-search-test: $(TEXT_SEARCH_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(TEXT_SEARCH_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,text-search-test))


# -------------- justify test program ----------------
JUSTIFY_OBJS := $(EDITOR_OBJS)

JUSTIFY_OBJS += justify-test.o
-include justify-test.d

justify-test: $(JUSTIFY_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(JUSTIFY_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,justify-test))


# -------------- command-runner test program ----------------
EDITOR_OBJS += command-runner.o
EDITOR_OBJS += command-runner.moc.o

COMMAND_RUNNER_OBJS := $(EDITOR_OBJS)

COMMAND_RUNNER_OBJS += command-runner-test.o
COMMAND_RUNNER_OBJS += command-runner-test.moc.o
-include command-runner-test.d
-include command-runner-test.moc.d

command-runner-test: $(COMMAND_RUNNER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(COMMAND_RUNNER_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,command-runner-test))


# ---------------------- named-td-test ----------------------
EDITOR_OBJS += doc-name.o
EDITOR_OBJS += host-and-resource-name.o
EDITOR_OBJS += host-name.o
EDITOR_OBJS += named-td.o

NAMED_TD_TEST_OBJS := $(EDITOR_OBJS)

NAMED_TD_TEST_OBJS += named-td-test.o
-include named-td-test.d

named-td-test: $(NAMED_TD_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(NAMED_TD_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,named-td-test))


# -------------------- named-td-list-test --------------------
EDITOR_OBJS += named-td-list.o

NAMED_TD_LIST_TEST_OBJS := $(EDITOR_OBJS)

NAMED_TD_LIST_TEST_OBJS += named-td-list-test.o
-include named-td-list-test.d

named-td-list-test: $(NAMED_TD_LIST_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(NAMED_TD_LIST_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,named-td-list-test))


# -------------------- nearby-file-test --------------------
EDITOR_OBJS += nearby-file.o

NEARBY_FILE_TEST_OBJS := $(EDITOR_OBJS)

NEARBY_FILE_TEST_OBJS += nearby-file-test.o
-include nearby-file-test.d

nearby-file-test: $(NEARBY_FILE_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(NEARBY_FILE_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,nearby-file-test))


# ------------------------ textcategory-test ---------------------------
textcategory-test: textcategory.o textcategory-test.o
	$(CXX) -o $@ $(CCFLAGS) $^ $(CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,textcategory-test))


# ----------------------- bufferlinesource-test ------------------------
EDITOR_OBJS += bufferlinesource.o

BUFFERLINESOURCE_TEST_OBJS := $(EDITOR_OBJS)

BUFFERLINESOURCE_TEST_OBJS += bufferlinesource-test.o
-include bufferlinesource-test.d

bufferlinesource-test: $(BUFFERLINESOURCE_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(BUFFERLINESOURCE_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,bufferlinesource-test))


# ------------------------- editor-fs-server ---------------------------
EDITOR_FS_SERVER_OBJS :=
EDITOR_FS_SERVER_OBJS += editor-fs-server.o
EDITOR_FS_SERVER_OBJS += vfs-local.o
EDITOR_FS_SERVER_OBJS += vfs-msg.o

-include editor-fs-server.d
-include vfs-local.d
-include vfs-msg.d

editor-fs-server.exe: $(EDITOR_FS_SERVER_OBJS) $(LIBSMBASE)
	@# Link this with -static because it gets invoked over an SSH
	@# connection, which makes it difficult to arrange for the proper
	@# library search path to be set.
	$(CXX) -o $@ -static $(CCFLAGS) $(EDITOR_FS_SERVER_OBJS) $(CONSOLE_LDFLAGS)

TOCLEAN += editor-fs-server.exe

all: editor-fs-server.exe


# ----------------------- editor-fs-server-test ------------------------
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


# ----------------------- makefile-hilite-test -------------------------
EDITOR_OBJS += makefile_hilite.yy.o

MAKEFILE_HILITE_TEST_OBJS := $(EDITOR_OBJS)

MAKEFILE_HILITE_TEST_OBJS += makefile-hilite-test.o
-include makefile-hilite-test.d

makefile-hilite-test: $(MAKEFILE_HILITE_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(MAKEFILE_HILITE_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,makefile-hilite-test))


# ---------------------- hashcomment-hilite-test -----------------------
EDITOR_OBJS += hashcomment_hilite.yy.o

HASHCOMMENT_HILITE_TEST_OBJS := $(EDITOR_OBJS)

HASHCOMMENT_HILITE_TEST_OBJS += hashcomment-hilite-test.o
-include hashcomment-hilite-test.d

hashcomment-hilite-test: $(HASHCOMMENT_HILITE_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(HASHCOMMENT_HILITE_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,hashcomment-hilite-test))


# -------------------------- ocaml-hilite-test -------------------------
EDITOR_OBJS += ocaml_hilite.yy.o

OCAML_HILITE_TEST_OBJS := $(EDITOR_OBJS)

OCAML_HILITE_TEST_OBJS += ocaml-hilite-test.o
-include ocaml-hilite-test.d

ocaml-hilite-test: $(OCAML_HILITE_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(OCAML_HILITE_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,ocaml-hilite-test))


# ------------------------- python-hilite-test -------------------------
EDITOR_OBJS += python_hilite.yy.o

PYTHON_HILITE_TEST_OBJS := $(EDITOR_OBJS)

PYTHON_HILITE_TEST_OBJS += python-hilite-test.o
-include python-hilite-test.d

python-hilite-test: $(PYTHON_HILITE_TEST_OBJS)
	$(CXX) -o $@ $(CCFLAGS) $(PYTHON_HILITE_TEST_OBJS) $(QT_CONSOLE_LDFLAGS)

$(eval $(call RUN_TEST_PROG,python-hilite-test))


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
EDITOR_OBJS += builtin-font.o
EDITOR_OBJS += connections-dialog.o
EDITOR_OBJS += connections-dialog.moc.o
EDITOR_OBJS += diff-hilite.o
EDITOR_OBJS += editor-global.moc.o
EDITOR_OBJS += editor-global.o
EDITOR_OBJS += editor-widget-frame.moc.o
EDITOR_OBJS += editor-widget-frame.o
EDITOR_OBJS += editor-widget.moc.o
EDITOR_OBJS += editor-widget.o
EDITOR_OBJS += editor-window.moc.o
EDITOR_OBJS += editor-window.o
EDITOR_OBJS += event-recorder.o
EDITOR_OBJS += event-replay.o
EDITOR_OBJS += event-replay.moc.o
EDITOR_OBJS += filename-input.o
EDITOR_OBJS += filename-input.moc.o
EDITOR_OBJS += fonts-dialog.o
EDITOR_OBJS += fonts-dialog.moc.o
EDITOR_OBJS += git-version.gen.o
EDITOR_OBJS += keybindings.doc.gen.o
EDITOR_OBJS += keys-dialog.o
EDITOR_OBJS += launch-command-dialog.o
EDITOR_OBJS += modal-dialog.o
EDITOR_OBJS += modal-dialog.moc.o
EDITOR_OBJS += my-table-widget.o
EDITOR_OBJS += my-table-widget.moc.o
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


# --------------------- misc ------------------------
clean:
	$(RM) $(TOCLEAN)
	$(RM) -r out
	$(RM) *.gcov *.gcda *.gcno


# These tests are run with the $(RETRY) prefix to automatically retry
# when they fail.  That is due to some race conditions that I have not
# been able to fully eliminate, mainly (perhaps exclusively) on Windows.
#
# One specific problem I am seeing is, a dialog will open but no widget
# has focus.  I suspect there may be a dependence on explorer.exe or
# something like that.
#
# Another problem is I set a window's title, but a subsequent query
# returns the old title for a short time afterward.
#
# NOTE: These tests cannot run in parallel because they pop up GUI
# windows on the display, so having more than one running at a time
# would cause interference due to the focus only being on one at a time.
#
check:
	@# The first test runs without any retry.
	./editor -ev=test/empty.ev
	@#
	@# Remaining tests have retry active.
	$(RETRY) ./editor -ev=test/down.ev test/file1.h
	$(RETRY) ./editor -ev=test/copy-paste.ev test/file1.h
	$(RETRY) ./editor -ev=test/file-open-dialog1.ev test/file1.h
	$(RETRY) ./editor -ev=test/file-open-nonexist.ev
	$(RETRY) ./editor -ev=test/simple-text-ins.ev
	$(RETRY) ./editor -ev=test/sar-find-cs-completion.ev test/file1.h
	$(RETRY) ./editor -ev=test/sar-repl-cs-completion.ev test/file1.h
	$(RETRY) ./editor -ev=test/sar-match-limit.ev test/file1.h
	$(RETRY) ./editor -ev=test/sar-replace-bol.ev test/file1.h
	$(RETRY) ./editor -ev=test/resize1.ev test/file1.h
	$(RETRY) ./editor -ev=test/read-only1.ev
	$(RETRY) ./editor -ev=test/read-only2.ev
	$(RETRY) ./editor -ev=test/read-only3.ev
	$(RETRY) ./editor -ev=test/prompt-unsaved-changes.ev
	$(RETRY) ./editor -ev=test/screenshot1.ev test/file1.h
	$(RETRY) ./editor -ev=test/visible-tabs.ev test/tabs-test.txt
	$(RETRY) ./editor -ev=test/search-hits-with-tab.ev test/tabs-test.txt
	cp test/file1.h tmp.h && $(RETRY) ./editor -ev=test/undo-redo-undo.ev tmp.h && rm tmp.h
	$(RETRY) ./editor -ev=test/keysequence.ev
	$(RETRY) ./editor -ev=test/keysequence2.ev
	cd test && sh ../retry.sh 3 ../editor -ev=fn-input-dialog-size.ev
	$(RETRY) ./editor -ev=test/screenshot-has-tabs.ev test/has-tabs.c
	$(RETRY) ./editor -ev=test/open-files-close-docs.ev test/read-only1.ev test/read-only2.ev test/read-only3.ev test/resize1.ev
	$(RETRY) ./editor -ev=test/open-files-filter.ev test/read-only1.ev test/read-only2.ev test/read-only3.ev test/resize1.ev
	$(RETRY) ./editor -ev=test/cut-then-paste.ev
	$(RETRY) ./editor -ev=test/select-beyond-eof.ev
	cp test/robotank.info.json.crlf.bin tmp.h && \
	  $(RETRY) ./editor -ev=test/sar-remove-ctlm.ev tmp.h && \
	  cmp tmp.h test/robotank.info.json.lf.bin && \
	  rm tmp.h
	$(RETRY) ./editor -ev=test/fn-input-repeat-tab.ev
	$(RETRY) ./editor -ev=test/reload-after-modification.ev test/gets-modified.txt
	$(RETRY) ./editor -ev=test/reload-from-files-dialog.ev test/gets-modified.txt test/gets-modified2.txt

# EOF
