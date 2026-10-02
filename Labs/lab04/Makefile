CXX=g++
CXXFLAGS = -std=c++17 -g
ROOT_DIR := $(shell git rev-parse --show-toplevel)
-include $(ROOT_DIR)/config/settings
-include $(DEP_DIR)/env

SOURCES = $(wildcard *.cpp)
OBJFILES = $(notdir $(SOURCES:.cpp=.o))
LIBS = -lcurl

all: transit_service

# Applicaiton Targets:
transit_service: $(OBJFILES)
	$(CXX) $(CXXFLAGS) $(OBJFILES) ${LIBS} -o $@

%.o: %.cpp %.h
	$(call make-depend-cxx,$<,$@,$(subst .o,.d,$@))
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Generate dependencies
make-depend-cxx=$(CXX) -MM -MF $3 -MP -MT $2 $(CXXFLAGS) $(INCLUDES) $1
-include $(addprefix $(OBJDIR)/,$(OBJFILES:.o=.d))

clean:
	rm -f *.o *.d transit_service






