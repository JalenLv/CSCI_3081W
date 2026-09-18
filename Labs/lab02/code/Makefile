EXEFILE := ducks
CXXFLAGS := -c -Wall
SOURCES := $(wildcard *.cpp)
OBJECTS := $(SOURCES:.cpp=.o)

.PHONY: clean

$(EXEFILE): $(OBJECTS)
	g++ $^ -o $@

%.o: %.cpp
	g++ $(CXXFLAGS) $^ -o $@

clean:
	rm -f *.o $(EXEFILE)