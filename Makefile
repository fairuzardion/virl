CXX := g++

# -Isrc lets every file use folder-qualified includes like
# "lexer/token.hpp" or "builtins/builtins.hpp" regardless of which
# subfolder the including file lives in.
#
# -Wno-maybe-uninitialized silences a known GCC false positive inside
# libstdc++'s std::variant machinery (seen with multi-alternative
# variants like Value); it is not a real bug in this codebase.
# -Wno-maybe-uninitialized silences a known GCC false positive inside
# libstdc++'s std::variant machinery (seen with multi-alternative
# variants like Value); it is not a real bug in this codebase.
#
# -Wno-dangling-reference silences a known GCC 13+ false positive that
# fires on helper functions returning `const T&` derived from a
# `const Value&` parameter (see src/builtins/builtins.cpp's
# argcheck::array/string) -- the referenced object is always a real
# vector element (args[i]), never a temporary, so there is no actual
# dangling reference.
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -Isrc -Wno-maybe-uninitialized -Wno-dangling-reference

SRC_DIR := src
BIN_DIR := bin
TARGET := $(BIN_DIR)/virl

# Every .cpp file under src/, found automatically -- adding a new
# builtins_*.cpp (or any new source file) needs no Makefile edit.
SOURCES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(BIN_DIR)/obj/%.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJECTS)

# -MMD/-MP generate a .d dependency file alongside each .o, so editing
# a header (e.g. value.hpp) correctly triggers a rebuild of every .cpp
# that includes it, without a hand-maintained dependency list.
$(BIN_DIR)/obj/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(DEPS)

run: all
	./$(TARGET) examples/hello.virl

clean:
	rm -rf $(BIN_DIR)
