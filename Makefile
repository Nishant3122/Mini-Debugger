# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -Wall -g

# Default target
all: New_file test

# Debugger program
New_file: New_file.cpp
	$(CXX) $(CXXFLAGS) New_file.cpp -o New_file

# Test program (NON-PIE is IMPORTANT)
test: test.cpp
	$(CXX) $(CXXFLAGS) -no-pie test.cpp -o test

# Clean
.PHONY: clean
clean:
	rm -f New_file test *.o
