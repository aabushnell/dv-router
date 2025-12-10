CXX = g++
CXXFLAGS = -g -Wall -pthread

MKDIR = mkdir -p

LINK_TARGET = bin/main

OBJS = \
	obj/main.o \
	obj/router.o \
	obj/sender.o \
	obj/receiver.o \
	obj/processor.o \
	obj/network.o

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

# helpers
clean:
	rm -rf obj bin $(REBUILDABLES)

start_all:
	sudo systemctl start container@routerA container@routerB container@routerC container@host1 container@host2

load_bin_all: load_bin_routerA load_bin_routerB load_bin_routerC load_bin_host1 load_bin_host2

load_bin_%:
	sudo cp bin/main /var/lib/nixos-containers/$*/root/router

run_bin_%:
	sudo nixos-container run $* -- /root/router
	
# dependency rules

obj/main.o: obj/router.o router.h

router.cpp: router.h

obj/router.o: obj/sender.o obj/receiver.o obj/processor.o

sender.cpp: sender.h

receiver.cpp: receiver.h

processor.cpp: processor.h
