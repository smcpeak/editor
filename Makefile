# Makefile for editor

# main target
all: buffer editor dialogs


# directories of other software
SMBASE := smbase


# C++ compiler, etc.
CXX := g++

# flags for the C and C++ compilers (and preprocessor)
CCFLAGS := -g -Wall -I/usr/X11/include -I$(SMBASE)

# flags for the linker
LDFLAGS := -g -Wall $(SMBASE)/libsmbase.a -L/usr/X11/lib -lX11


# patterns of files to delete in the 'clean' target
TOCLEAN =


# compile .cc to .o
TOCLEAN += *.o *.d
%.o : %.cc
	$(CXX) -c -o $@ $< $(CCFLAGS)
	@perl $(SMBASE)/depend.pl -o $@ $< $(CCFLAGS) > $*.d


# -------------- buffer test program ----------------
TOCLEAN += buffer buffer.tmp
BUFFER_OBJS = buffer.cc textline.o position.o
buffer: array.h $(BUFFER_OBJS)
	$(CXX) -o $@ $(CCFLAGS) -DTEST_BUFFER $(BUFFER_OBJS) $(LDFLAGS)


# ------------------ the editor ---------------------
TOCLEAN += editor
EDITOR_OBJS = editor.o buffer.o textline.o position.o
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
