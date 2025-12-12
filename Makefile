CXX = g++
CXXFLAGS = -Wall -g

all: main testprog

main: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o main

testprog: testprog.cpp
	$(CXX) $(CXXFLAGS) testprog.cpp -o testprog

clean:
	rm -f main testprog
