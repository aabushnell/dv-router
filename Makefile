CXX = g++
CXXFLAGS = -g -Wall

MKDIR = mkdir -p

LINK_TARGET = bin/router

OBJS = \
	obj/main.o

REBUILDABLES = $(OBJS) $(LINK_TARGET)

all: $(LINK_TARGET)

$(LINK_TARGET) : $(OBJS) | bin
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o : %.cpp | obj
	$(CXX) $(CXXFLAGS) -o $@ -c $<

obj:
	$(MKDIR) $@

bin:
	$(MKDIR) $@

clean:
	rm -rf obj bin $(REBUILDABLES)
