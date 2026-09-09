CXX ?= g++
CXXFLAGS ?= -std=c++23 -O2 -Wall -Wextra

all: test bench

test: test.cpp alloc.hpp
	$(CXX) $(CXXFLAGS) test.cpp -o $@

bench: bench.cpp alloc.hpp
	$(CXX) $(CXXFLAGS) -DNDEBUG bench.cpp -o $@

run: test
	./test

clean:
	rm -f test bench

.PHONY: all run clean
