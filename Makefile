# Makefile for editor

# Default target.
all: comment.yy.cc
all: editor.exe
all: test-prog-outs


# directories of other software
SMBASE   := smbase
SMFLEX   := smflex
SMQTUTIL := smqtutil
ASTGEN   := ast


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

# Python interpreter.
PYTHON3 = python3

# https://mypy-lang.org/
MYPY = mypy

# If 1, hook the `mypy` checks into the `all` target.
ENABLE_MYPY = 0

# Pull in Qt configuration from smqtutil, which provides
# QT5INCLUDE, QT5LIB and QT5BIN.
include $(SMQTUTIL)/config.mk

# User customizations.
-include config.mk

# Set QT_CCFLAGS, QT_LDFLAGS, QT_TOCLEAN, and the 'moc' rule.
include $(SMQTUTIL)/qtvars.mk


# Flags for the C and C++ compilers (and preprocessor).
#
# C++17 is needed for `std::string_view`, used by smbase.
CCFLAGS := -g -Wall -std=c++17

# Explicitly add the current directory to the include search path so
# when files in smqtutil include "smbase/...", that will work.
CCFLAGS += -I.

# #includes in the `editor` repository should all use explicit directory
# prefixes to access files in subdirectories, so they are not passed as
# -I arguments.

CCFLAGS += $(QT_CCFLAGS)
CCFLAGS += $(EXTRA_CCFLAGS)

ifeq ($(COVERAGE),1)
  CCFLAGS += -fprofile-arcs -ftest-coverage
endif

LIBSMBASE   := $(SMBASE)/obj/libsmbase.a
LIBSMQTUTIL := $(SMQTUTIL)/libsmqtutil.a
LIBASTGEN   := $(ASTGEN)/libast.a

# Libraries for console programs, suitable for use as a dependency in a
# recipe.
CONSOLE_LIBRARIES :=
CONSOLE_LIBRARIES += $(LIBSMBASE)

# Flags for the linker for console programs.
CONSOLE_LDFLAGS := -g
CONSOLE_LDFLAGS += $(CONSOLE_LIBRARIES)
CONSOLE_LDFLAGS += $(EXTRA_LDFLAGS)

# Libraries for GUI programs, usable as dependencies.
GUI_LIBRARIES :=
GUI_LIBRARIES += $(LIBSMQTUTIL)
GUI_LIBRARIES += $(LIBASTGEN)
GUI_LIBRARIES += $(LIBSMBASE)

# Link flags for GUI programs.
GUI_LDFLAGS := -g
GUI_LDFLAGS += $(GUI_LIBRARIES)
GUI_LDFLAGS += $(QT_LDFLAGS)
GUI_LDFLAGS += $(EXTRA_LDFLAGS)

# Link flags for console programs that use Qt5Core.  Specifically,
# I am using QRegularExpression in the 'justify' module.
QT_CONSOLE_LDFLAGS := -g
QT_CONSOLE_LDFLAGS += $(GUI_LIBRARIES)
QT_CONSOLE_LDFLAGS += -L$(QT5LIB) -lQt5Core -Wl,-rpath=$(QT5LIB)
QT_CONSOLE_LDFLAGS += $(EXTRA_LDFLAGS)


# Create a target to run $1 and save the output to out/$1.out.
#
# The target is a .ok file instead of the .out itself because I do not
# want 'make' to delete the .out file if the command fails, since I want
# to be able to inspect it after the failure.
#
define RUN_TEST_PROG
test-prog-outs: out/$1.ok
out/$1.ok: $1.exe
	$$(CREATE_OUTPUT_DIRECTORY)
	$(RUN_WITH_TIMEOUT) ./$1.exe </dev/null >out/$1.out 2>&1
	touch $$@
endef


# Target to run the test programs.
.PHONY: test-prog-outs


# patterns of files to delete in the 'clean' target; targets below
# add things to this using "+="
TOCLEAN = $(QT_TOCLEAN)
TOCLEAN += *.exe


# Object files for the editor.  This is built up gradually as needed
# for various test programs, roughly in bottom-up order.
EDITOR_OBJS :=


# Command prefix to retry a test up to 3 times.
RETRY := sh ./retry.sh 3


# Command to generate *.o.json files, which then are combined to make
# compile_commands.json.
MAKE_CC_JSON = $(PYTHON3) $(SMBASE)/make-cc-json.py


# ---------------- pattern rules --------------------
# compile .cc to .o
# -MMD causes GCC to write .d file.
# The -MP modifier adds phony targets to deal with removed headers.
TOCLEAN += *.o *.d *.o.json
%.o : %.cc
	$(CXX) -c -MMD -MP -o $@ $(CCFLAGS) $<
	$(MAKE_CC_JSON) $(CXX) -c -MMD -MP -o $@ $(CCFLAGS) $< >$@.json


# Pull in the automatic dependencies.
-include $(wildcard *.d)


# Clean generated sources.
TOCLEAN += *.gen.*

# Encode help files as C string literals.
%.doc.gen.cc %.doc.gen.h: doc/%.txt
	perl $(SMBASE)/file-to-strlit.pl doc_$* $^ $*.doc.gen.h $*.doc.gen.cc

# Run astgen to generate C++ data classes.
%.ast.gen.fwd.h %.ast.gen.h %.ast.gen.cc: %.ast $(ASTGEN)/astgen.exe
	$(ASTGEN)/astgen.exe -o$*.ast.gen $<


# Generate a highlighting lexer from a (sm)flex specification.
#
# -b makes lex.backup.
#
# TODO: The -b option is incompatible with parallel build, so I have
# removed it.  I should fix smflex and then re-enable this.
#
# NOTE: The base name of the lexer files can only use characters that
# are allowed in C identifiers because Flex derives the name of some
# internal identifiers from that file name.
TOCLEAN += *.yy.cc *.yy.h *.lex.backup
%.yy.cc: %.lex %.h
	$(SMFLEX)/smflex -o$@ -P$*_yy $*.lex
	@#mv lex.backup $*.lex.backup
	@#cat $*.lex.backup


# ------------------------------- astgen -------------------------------
$(ASTGEN)/astgen.exe:
	@echo "Need $(ASTGEN)/astgen.exe.  Run `make` in $(ASTGEN)."
	@exit 2


# ----------------------------- unit-tests -----------------------------
EDITOR_OBJS += bufferlinesource.o
EDITOR_OBJS += c_hilite.yy.o
EDITOR_OBJS += command-runner.moc.o
EDITOR_OBJS += command-runner.o
EDITOR_OBJS += comment.yy.o
EDITOR_OBJS += doc-name.o
EDITOR_OBJS += doc-type-detect.o
EDITOR_OBJS += editor-strutil.o
EDITOR_OBJS += fasttime.o
EDITOR_OBJS += gap-gdvalue.o
EDITOR_OBJS += hashcomment_hilite.yy.o
EDITOR_OBJS += hilite.o
EDITOR_OBJS += history.o
EDITOR_OBJS += host-and-resource-name.o
EDITOR_OBJS += host-name.o
EDITOR_OBJS += justify.o
EDITOR_OBJS += lex_hilite.o
EDITOR_OBJS += lsp-client.moc.o
EDITOR_OBJS += lsp-client.o
EDITOR_OBJS += lsp-conv.o
EDITOR_OBJS += lsp-data.o
EDITOR_OBJS += lsp-manager.moc.o
EDITOR_OBJS += lsp-manager.o
EDITOR_OBJS += lsp-symbol-request-kind.o
EDITOR_OBJS += makefile_hilite.yy.o
EDITOR_OBJS += named-td-editor.o
EDITOR_OBJS += named-td-list.o
EDITOR_OBJS += named-td.o
EDITOR_OBJS += nearby-file.o
EDITOR_OBJS += ocaml_hilite.yy.o
EDITOR_OBJS += python_hilite.yy.o
EDITOR_OBJS += td-change.o
EDITOR_OBJS += td-core.o
EDITOR_OBJS += td-diagnostics.o
EDITOR_OBJS += td-editor.o
EDITOR_OBJS += td-line.o
EDITOR_OBJS += td-obs-recorder.o
EDITOR_OBJS += td.o
EDITOR_OBJS += text-search.o
EDITOR_OBJS += textcategory.o
EDITOR_OBJS += textlcoord.o
EDITOR_OBJS += textmcoord-map.o
EDITOR_OBJS += textmcoord.o
EDITOR_OBJS += uri-util.o
EDITOR_OBJS += vfs-connections.moc.o
EDITOR_OBJS += vfs-connections.o
EDITOR_OBJS += vfs-msg.o
EDITOR_OBJS += vfs-query.moc.o
EDITOR_OBJS += vfs-query.o
EDITOR_OBJS += waiting-counter.o

UNIT_TESTS_OBJS := $(EDITOR_OBJS)

UNIT_TESTS_OBJS += bufferlinesource-test.o
UNIT_TESTS_OBJS += c-hilite-test.o
UNIT_TESTS_OBJS += command-runner-test.moc.o
UNIT_TESTS_OBJS += command-runner-test.o
UNIT_TESTS_OBJS += doc-type-detect-test.o
UNIT_TESTS_OBJS += editor-fs-server-test.moc.o
UNIT_TESTS_OBJS += editor-fs-server-test.o
UNIT_TESTS_OBJS += editor-strutil-test.o
UNIT_TESTS_OBJS += gap-test.o
UNIT_TESTS_OBJS += hashcomment-hilite-test.o
UNIT_TESTS_OBJS += justify-test.o
UNIT_TESTS_OBJS += lsp-client-test.moc.o
UNIT_TESTS_OBJS += lsp-client-test.o
UNIT_TESTS_OBJS += lsp-conv-test.o
UNIT_TESTS_OBJS += lsp-data-test.o
UNIT_TESTS_OBJS += lsp-manager-test.moc.o
UNIT_TESTS_OBJS += lsp-manager-test.o
UNIT_TESTS_OBJS += lsp-test-request-params.o
UNIT_TESTS_OBJS += makefile-hilite-test.o
UNIT_TESTS_OBJS += named-td-editor-test.o
UNIT_TESTS_OBJS += named-td-list-test.o
UNIT_TESTS_OBJS += named-td-test.o
UNIT_TESTS_OBJS += nearby-file-test.o
UNIT_TESTS_OBJS += ocaml-hilite-test.o
UNIT_TESTS_OBJS += python-hilite-test.o
UNIT_TESTS_OBJS += td-core-test.o
UNIT_TESTS_OBJS += td-diagnostics-test.o
UNIT_TESTS_OBJS += td-editor-test.o
UNIT_TESTS_OBJS += td-line-test.o
UNIT_TESTS_OBJS += td-obs-recorder-test.o
UNIT_TESTS_OBJS += text-search-test.o
UNIT_TESTS_OBJS += textcategory-test.o
UNIT_TESTS_OBJS += textmcoord-map-test.o
UNIT_TESTS_OBJS += unit-tests.o
UNIT_TESTS_OBJS += uri-util-test.o
UNIT_TESTS_OBJS += vfs-connections-test.moc.o
UNIT_TESTS_OBJS += vfs-connections-test.o

unit-tests.exe: $(UNIT_TESTS_OBJS) $(GUI_LIBRARIES)
	@# I link with `GUI_LDFLAGS` because lots of modules that need
	@# to link with GUI elements of Qt are included among the
	@# dependencies even though they won't get run by these tests.
	$(CXX) -o $@ $(CCFLAGS) $(UNIT_TESTS_OBJS) $(GUI_LDFLAGS)

$(eval $(call RUN_TEST_PROG,unit-tests))

# Some of the unit tests (e.g., vfs-connections-test) require the server
# executable.
out/unit-tests.ok: editor-fs-server.exe

# `td-core-test` might leave this file (but only if the test fails).
TOCLEAN += td-core.tmp


# ------------------------- editor-fs-server ---------------------------
EDITOR_FS_SERVER_OBJS :=
EDITOR_FS_SERVER_OBJS += editor-fs-server.o
EDITOR_FS_SERVER_OBJS += editor-version.o
EDITOR_FS_SERVER_OBJS += git-version.gen.o
EDITOR_FS_SERVER_OBJS += vfs-local.o
EDITOR_FS_SERVER_OBJS += vfs-msg.o

editor-fs-server.exe: $(EDITOR_FS_SERVER_OBJS) $(CONSOLE_LIBRARIES)
	@# Link this with -static because it gets invoked over an SSH
	@# connection, which makes it difficult to arrange for the proper
	@# library search path to be set.
	@#
	@# On Linux, this causes the warning:
	@#
	@#   warning: Using 'getpwuid' in statically linked applications requires at runtime the shared libraries from the glibc version used for linking
	@#
	@# That does not matter in this program since the FS server
	@# never queries the current username, but there's no simple way
	@# to suppress it, so I just have to live with it.
	@#
	$(CXX) -o $@ -static $(CCFLAGS) $(EDITOR_FS_SERVER_OBJS) $(CONSOLE_LDFLAGS)

all: editor-fs-server.exe


# ---------------------- VFS localhost SSH tests -----------------------
# Optionally run tests that use SSH to connect to localhost.
ifeq ($(TEST_SSH_LOCALHOST),1)

test-prog-outs: out/vfs-connections-test-localhost.ok
out/vfs-connections-test-localhost.ok: unit-tests.exe editor-fs-server.exe
	$(CREATE_OUTPUT_DIRECTORY)
	$(RUN_WITH_TIMEOUT) ./unit-tests.exe vfs_connections localhost \
	  </dev/null >out/vfs-connections-test-localhost.out 2>&1
	touch $@

test-prog-outs: out/editor-fs-server-test-localhost.ok
out/editor-fs-server-test-localhost.ok: unit-tests.exe editor-fs-server.exe
	$(CREATE_OUTPUT_DIRECTORY)
	$(RUN_WITH_TIMEOUT) ./unit-tests.exe editor_fs_server localhost \
	  </dev/null >out/editor-fs-server-test-localhost.out 2>&1
	touch $@

endif # TEST_SSH_LOCALHOST


# ----------------------------- gui-tests ------------------------------
EDITOR_OBJS += completions-dialog.o
EDITOR_OBJS += completions-dialog.moc.o
EDITOR_OBJS += connections-dialog.o
EDITOR_OBJS += connections-dialog.moc.o
EDITOR_OBJS += diagnostic-details-dialog.moc.o
EDITOR_OBJS += diagnostic-details-dialog.o
EDITOR_OBJS += editor-proxy-style.o
EDITOR_OBJS += modal-dialog.o
EDITOR_OBJS += modal-dialog.moc.o
EDITOR_OBJS += pixmaps.o

GUI_TESTS_OBJS := $(EDITOR_OBJS)
GUI_TESTS_OBJS += completions-dialog-test.o
GUI_TESTS_OBJS += connections-dialog-test.o
GUI_TESTS_OBJS += diagnostic-details-dialog-test.o
GUI_TESTS_OBJS += gui-tests.o
GUI_TESTS_OBJS += resources.qrc.gen.o

gui-tests.exe: $(GUI_TESTS_OBJS) $(GUI_LIBRARIES)
	$(CXX) -o $@ $(CCFLAGS) $(GUI_TESTS_OBJS) $(GUI_LDFLAGS)

# This has to be run manually.
all: gui-tests.exe


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


# ------------------------- extra dependencies -------------------------
# These dependencies ensure that automatically-generated code is
# created in time to be used by other build processes which need it.

# Arguments to find-extra-deps.py.
EXTRADEPS_ARGS :=

# Raw dependencies to scan.
EXTRADEPS_ARGS += *.d

.PHONY: remake-extradep
remake-extradep:
	$(PYTHON3) $(SMBASE)/find-extra-deps.py $(EXTRADEPS_ARGS) >extradep.mk

include extradep.mk

check: validate-extradep

.PHONY: validate-extradep
validate-extradep: all
	$(PYTHON3) $(SMBASE)/find-extra-deps.py $(EXTRADEPS_ARGS) >extradep.tmp
	@echo diff extradep.mk extradep.tmp
	@if diff extradep.mk extradep.tmp; then true; else \
	  echo "If the extra files are meant to be generated during"; \
	  echo "the build, run 'make remake-extradep'.  If they are"; \
	  echo "meant to be checked in, 'git add' them now."; \
	  exit 2; \
	fi
	rm extradep.tmp


# ------------------ the editor ---------------------
# The following list of object files is added to what has been set
# above.  Above, modules are pulled in as needed in order to build
# various test programs.  What's left over is the modules that are not
# part of any test program.
EDITOR_OBJS += apply-command-dialog.moc.o
EDITOR_OBJS += apply-command-dialog.o
EDITOR_OBJS += builtin-font.o
EDITOR_OBJS += diff-hilite.o
EDITOR_OBJS += editor-command.ast.gen.o
EDITOR_OBJS += editor-global.moc.o
EDITOR_OBJS += editor-global.o
EDITOR_OBJS += editor-settings.o
EDITOR_OBJS += editor-version.o
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
EDITOR_OBJS += lsp-status-widget.moc.o
EDITOR_OBJS += lsp-status-widget.o
EDITOR_OBJS += macro-creator-dialog.o
EDITOR_OBJS += macro-creator-dialog.moc.o
EDITOR_OBJS += macro-run-dialog.o
EDITOR_OBJS += macro-run-dialog.moc.o
EDITOR_OBJS += open-files-dialog.o
EDITOR_OBJS += open-files-dialog.moc.o
EDITOR_OBJS += process-watcher.o
EDITOR_OBJS += process-watcher.moc.o
EDITOR_OBJS += resources.qrc.gen.o
EDITOR_OBJS += sar-panel.o
EDITOR_OBJS += sar-panel.moc.o
EDITOR_OBJS += status-bar.o
EDITOR_OBJS += styledb.o
EDITOR_OBJS += textinput.o
EDITOR_OBJS += textinput.moc.o
EDITOR_OBJS += vfs-query-sync.o
EDITOR_OBJS += vfs-query-sync.moc.o

editor.exe: $(EDITOR_OBJS) $(GUI_LIBRARIES)
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_OBJS) $(GUI_LDFLAGS)


# ----------------------- compile_commands.json ------------------------
# The rules in this section are copied from smbase/Makefile (and then
# slightly modified).
.PHONY: compile_commands.json
compile_commands.json:
	(echo "["; cat *.o.json | sed '$$ s/,$$//'; echo "]") > $@


# -------------- check create-tuple-class.py outputs -------------------
# Set of header files that use create-tuple-class.py.
CTC_HEADERS :=
CTC_HEADERS += lsp-data.h

# Corresponding implementation files.
CTC_IMPL_FILES := $(CTC_HEADERS:.h=.cc)

# Check that all of the CTC-generated code is up to date.
out/ctc-up-to-date.ok: $(CTC_HEADERS) $(CTC_IMPL_FILES) $(SMBASE)/create-tuple-class.py
	$(CREATE_OUTPUT_DIRECTORY)
	$(PYTHON3) $(SMBASE)/create-tuple-class.py \
	  --check $(CTC_HEADERS)
	touch $@

all: out/ctc-up-to-date.ok


# ------------------------------- mypy ---------------------------------
# Run `mypy` on a script.
out/%.mypy.ok: %
	$(CREATE_OUTPUT_DIRECTORY)
	$(MYPY) --strict $*
	touch $@

.PHONY: check-mypy
check-mypy: out/lsp-test-server.py.mypy.ok

ifeq ($(ENABLE_MYPY),1)
all: check-mypy
endif


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
	./editor.exe -ev=test/empty.ev
	@#
	@# Run the big batch of tests that all run in one process, with
	@# internal retry capabilities.
	./editor.exe -testCommands=test/all-tests.gdvn
	@#
	@# Remaining tests that can't easily run in the big batch.
	cp test/file1.h tmp.h && $(RETRY) ./editor.exe -ev=test/undo-redo-undo.ev tmp.h && rm tmp.h
	cd test && sh ../retry.sh 3 ../editor.exe -ev=fn-input-dialog-size.ev
	cp test/robotank.info.json.crlf.bin tmp.h && \
	  $(RETRY) ./editor.exe -ev=test/sar-remove-ctlm.ev tmp.h && \
	  cmp tmp.h test/robotank.info.json.lf.bin && \
	  rm tmp.h
	@#
	@# There isn't an easy way to do this automatically elsewhere,
	@# mainly due to how my test programs are built, so remake
	@# `compile_commands.json` here, since this way it will at least
	@# happen somewhat periodically, and at this point we know that
	@# none of the *.o.json files is missing or in the middle of
	@# being written.
	$(MAKE) compile_commands.json

# EOF
