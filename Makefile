# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Iinclude -g -O0 -Werror  # Treat all warnings as errors

# Directories
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
SRC_DIR = src
INCLUDE_DIR = include

# Files
TARGET = $(BIN_DIR)/example_program
SRC = $(SRC_DIR)/libswd.cpp example.cpp
OBJ = $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Default target
all: $(OBJ_DIR) $(BIN_DIR) $(TARGET)

# Link the program
$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -I$(INCLUDE_DIR) -o $(TARGET)

# Compile .cpp files to .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create the necessary build directories
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean the entire build directory
clean:
	rm -rf $(BUILD_DIR)

# Debug configuration option
debug: CXXFLAGS += -g -O0
debug: clean all

# Release configuration (if desired in the future)
release: CXXFLAGS += -O2
release: clean all

.PHONY: all clean debug release
