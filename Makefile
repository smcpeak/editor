# Makefile for editor

#tmp: testgap buffer

# main target
all: testgap buffer editor


# directories of other software
SMBASE := smbase


# C++ compiler, etc.
CXX := g++

# flags for the C and C++ compilers (and preprocessor)
CCFLAGS := -g -Wall -I$(QTDIR)/include -I$(SMBASE)

# flags for the linker
LDFLAGS := -g -Wall $(SMBASE)/libsmbase.a -lqt


# patterns of files to delete in the 'clean' target
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
	$(CXX) -o $@ $(CCFLAGS) testgap.cc -g -Wall $(SMBASE)/libsmbase.a
	./testgap >/dev/null 2>&1


# -------------- buffer test program ----------------
TOCLEAN += buffer buffer.tmp
BUFFER_OBJS := buffer.cc
buffer: gap.h $(BUFFER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_BUFFER $(BUFFER_OBJS) -g -Wall $(SMBASE)/libsmbase.a
	./buffer >/dev/null 2>&1


# ------------------ the editor ---------------------
TOCLEAN += editor
EDITOR_OBJS := \
  editor.o \
  moc_editor.o \
  buffer.o \
  main.o \
  moc_main.o \
  bufferstate.o
editor: $(EDITOR_OBJS) buffer.h textline.h position.h
	$(CXX) -o $@ $(CCFLAGS) $(EDITOR_OBJS) $(LDFLAGS)
-include $(EDITOR_OBJS:.o=.d)


# -------------- another test program ---------------
TOCLEAN += dialogs
dialogs: dialogs.cc
	$(CXX) -o $@ $(CCFLAGS) dialogs.cc -lXm -lXt $(LDFLAGS)


# --------------------- misc ------------------------
clean:
	rm -f $(TOCLEAN)
