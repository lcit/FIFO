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

all: test_functional_FIFO test_performance_FIFO #test_sFIFO

test_performance_FIFO: test_performance_FIFO.cpp
	$(CPP) $(CPPFLAGS) -o test_performance_FIFO test_performance_FIFO.cpp FIFO.hpp $(OBJS) $(LDFLAGS)

test_functional_FIFO: test_functional_FIFO.cpp
	$(CPP) $(CPPFLAGS) -o test_functional_FIFO test_functional_FIFO.cpp FIFO.hpp $(OBJS) $(LDFLAGS)

test_sFIFO: test_sFIFO.cpp
	$(CPP) $(CPPFLAGS) -o test_sFIFO test_sFIFO.cpp sFIFO.hpp $(OBJS) $(LDFLAGS)

clean:
	-rm -f *.o; rm test_FIFO; rm test_sFIFO; rm test_performance_FIFO; rm test_functional_FIFO
