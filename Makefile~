# =========================================================================
# Author: Leonardo Citraro
# Company: 
# Filename: Makefile	
# =========================================================================
# /////////////////////////////////////////////////////////////////////////
CPP         = g++
INCLUDES    = -I.
CPPFLAGS    = -std=c++14 -pthread  $(INCLUDES)
DEPS        = 
OBJS        = 
LDFLAGS     = -g $(DEPS)
# /////////////////////////////////////////////////////////////////////////

all: test_FIFO test_sFIFO

test_FIFO: test_FIFO.cpp
	$(CPP) $(CPPFLAGS) -o test_FIFO test_FIFO.cpp FIFO.hpp $(OBJS) $(LDFLAGS)

test_sFIFO: test_sFIFO.cpp
	$(CPP) $(CPPFLAGS) -o test_sFIFO test_sFIFO.cpp sFIFO.hpp $(OBJS) $(LDFLAGS)

clean:
	-rm -f *.o; rm test_FIFO; rm test_sFIFO
