# Test su PC: `make -C tests` dalla radice del progetto.
CXX      ?= g++
CXXFLAGS ?= -std=c++11 -O1 -Wall -Wextra -Wpedantic -Wconversion -Wno-float-conversion

.PHONY: test clean
test: test_logic
	./test_logic

test_logic: test_logic.cpp ../wauf_logic.h
	$(CXX) $(CXXFLAGS) -o $@ test_logic.cpp

clean:
	rm -f test_logic
