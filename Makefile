CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++20 -O2

SOURCES = $(wildcard *.cpp)
EXECS = $(SOURCES:%.cpp=%)
HEADERS = $(wildcard *.h)

all: $(EXECS)

%: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(EXECS)

.PHONY: all clean
