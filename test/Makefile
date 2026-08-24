CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Werror -O1 -g
SRCS     := $(wildcard *.cpp)

.PHONY: test clean
test: unit_tests
	./unit_tests

unit_tests: $(SRCS) test_main.h $(wildcard ../CardputerRFScope/*.h)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $@

clean:
	rm -f unit_tests
