CC = gcc
CFLAGS = -g -Wall -pthread

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
	$(CC) $(CFLAGS) -o $@ $^

obj/%.o: %.c | obj
	$(CC) $(CFLAGS) -o $@ -c $<

obj:
	$(MKDIR) $@

bin:
	$(MKDIR) $@

# helpers
clean:
	rm -rf obj bin $(REBUILDABLES)

start_all:
	sudo systemctl start container@routerA container@routerB container@routerC container@host1 container@host2

stop_all:
	sudo systemctl stop container@routerA container@routerB container@routerC container@host1 container@host2

restart_all:
	sudo systemctl restart container@routerA container@routerB container@routerC container@host1 container@host2

load_all: load_bin_routerA load_bin_routerB load_bin_routerC

load_bin_%:
	sudo cp bin/main /var/lib/nixos-containers/$*/root/router

run_all: run_bin_routerA run_bin_routerB run_bin_routerC

run_bin_%:
	sudo nixos-container run $* -- /root/router
	
# dependency rules

obj/main.o: main.c router.h

obj/router.o: router.c router.h sender.h receiver.h processor.h network.h

obj/sender.o: sender.c sender.h router.h

obj/receiver.o: receiver.c receiver.h router.h

obj/processor.o: processor.c processor.h router.h network.h

obj/network.o: network.c network.h
