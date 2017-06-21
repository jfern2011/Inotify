# Assign the compiler that we want to use:
CC=g++

#----------------------------------------------------------------------
# Include directories:
#----------------------------------------------------------------------
IDIRS = ../include ../../abort ../../signal ../../types ../../util \
		../../event_sink/include

# Object file directory:
ODIR=../obj

# Assign compiler options:
CFLAGS=-c -g -Wall -Wall -Wno-unused-function \
					$(foreach dir, $(IDIRS), -I$(dir)) --std=c++11

#----------------------------------------------------------------------
# Header dependencies:
#----------------------------------------------------------------------
_DEPS = Inotify.h abort.h Signal.h types.h util.h WriteEventSink.h

DEPS = $(join $(addsuffix /, $(IDIRS)), $(_DEPS))

# Object files:
_OBJ = Inotify_ut.o
OBJ = $(patsubst %, $(ODIR)/%, $(_OBJ))

# Generate object files
$(ODIR)/%.o: %.cpp $(DEPS)
	@ if ! [ -d $(ODIR) ]; then mkdir $(ODIR); fi
	@ $(CC) -c -o $@ $< $(CFLAGS)

# Build the unit test
test: $(OBJ)
	$(CC) -g -o $@ $^

clean:
	@ rm $(ODIR)/*.o test

# This target is always out-of-date
.PHONY: clean++

clean++:
	@ rm -rf $(ODIR) *~ core $(IDIR)/*~ test
	@ echo clean++: all clean!
