TARGET := test
CXX ?= g++
SRC := test.cpp
OBJ := $(SRC:.cpp=.o)
DEPS := $(SRC:.cpp=.d)

# tweak as you like
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -MMD -MP

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

# Rebuild when the header changes; -MMD/-MP also generate precise deps
%.o: %.cpp alloc.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

debug: CXXFLAGS := -std=c++20 -O0 -g3 -Wall -Wextra -Wpedantic -MMD -MP
debug: clean all

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(DEPS) $(TARGET)

-include $(DEPS)

.PHONY: all debug run clean
