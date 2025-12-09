CXX = g++
CXXFLAGS = -g -Wall -pthread

MKDIR = mkdir -p

LINK_TARGET = bin/main

OBJS = \
	obj/main.o \
	obj/router.o \
	obj/sender.o \
	obj/receiver.o \
	obj/processor.o

REBUILDABLES = $(OBJS) $(LINK_TARGET)

all: $(LINK_TARGET)

$(LINK_TARGET): $(OBJS) | bin
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o: %.cpp | obj
	$(CXX) $(CXXFLAGS) -o $@ -c $<

obj:
	$(MKDIR) $@

bin:
	$(MKDIR) $@

clean:
	rm -rf obj bin $(REBUILDABLES)

# dependency rules

main.o: router.o router.h

router.cpp: router.h

router.o: sender.o receiver.o processor.o

sender.cpp: sender.h

receiver.cpp: receiver.h

processor.cpp: processor.h
