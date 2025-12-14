# DV-Router

Author: Aaron Bushnell

## Building

The routing program binary can be built by running
`make all` which compiles and links the components into
a single `main` binary in the bin folder.

Some additional helper make commands are implemented
such as `make load_bin_<x>` which can load the compiled
binary into the supplied container name as `x`. Similarly,
`make run_bin_<x>` runs the router binary inside the specified
container.

Moreover, `make start_all`, `make stop_all`, and `make restart_all` issues
the relevant commands to systemd to start, stop, or restart all containers
respectively.

## Network Configuration

In order to simulate multiple devices (routers and hosts) forming a network,
I implemented containers for each virtual device on my computer running
NixOS. The full `simulation.nix` file can be found in this repo for reference.
It is meant to be used as a flake module within a larger NixOS configuration.
The main points of the setup will be outlined below.

### Containers

The simulation consists of NixOS containers, which are each given their own
private network and corresponding network namespace in the linux kernel.
The protocol port (_5555_) is allowed in the containers firewalls.

The basic simulation contains three router containers:

- routerA
- routerB
- routerC

Each of the three routers is configured through systemd to run `sleep infinity`
on creation. An identical binary for the routing program is then loaded
and run on each individual router to build the routing table. The containers
are also loaded with basic network utilities like `socat` and `tcpdump` and
can execute shell commands while running.

It also contains two 'hosts' meant to be leaf nodes in the network:

- host1
- host2

These containers are not meant to run the routing program but exist merely
to test the results of the implemented routing table(s). At container creation
they have no way to communicate and must rely on the routers to create
ip routes in the kernel to forward messages between hosts.

### Topology

Four networking bridges are created:

- linkHost1:
  - connects Host1 <-> RouterA
  - contains addresses 192.168.1.0/24
  - bound to interface ethH1 in host1
  - bound to interface ethA0 in routerA
- linkAB:
  - connects RouterA <-> RouterB
  - contains addresses 10.1.0.0/24
  - bound to interface ethA1 in routerA
  - bound to interface ethB1 in routerB
- linkBC:
  - connects RouterB <-> RouterC
  - contains addresses 10.2.0.0/24
  - bound to interface ethB2 in routerB
  - bound to interface ethC2 in routerC
- linkHost2:
  - connects RouterC <-> Host2
  - contains addresses 192.168.2.0/24
  - bound to interface ethC3 in routerC
  - bound to interface ethH2 in host2

### Services

The biggest challenge in implementing this configuration was getting the
NetworkManager service to stop killing my interfaces. I eventually
set all `eth*` interfaces to unmanaged status as I don't need any ethernet
interfaces on the host system. Within the containers networking is managed
by the networkd daemon.

## Routing Program

The routing program was written in c++, although the core logic of the
distance vector program only uses vanilla c syntax to align with the
prescribed specifications (i.e. I use only `char *` wherever specified)
and build the internal data structure tables out of c-style linked lists.

The program consists of four threads outlined below.

### Main Thread

The main router thread initializes all of the necessary data structures
for the program and starts the other threads. The most substantial component
of the startup logic is to identify all available interfaces and bind them
to sockets. The interfaces and sockets are stored in vectors containing
their relevant data fields and passed to the other threads spawned by the
router.

After the main thread spawns its worker threads it sleeps, occasionally
checking for the liveness of neighboring routers and updating the internal
routing table as needed. There is no compelling reason for why the main
thread does this other than the fact that it has no other responsibilities
after startup and this logic did not fit cleanly into the roles of the
worker threads.

### Sender Thread

The sender thread loops, waking up every five seconds, to send out HELLO
messages, and, if needed, DV updates. Before sending out messages it also
updates the table of direct neighbors, specifically checking if any of the
links are 'dead'. If so, a boolean flag is set for the main loop to process.

### Receiver Thread

The receiver thread constantly checks for any pending messages on any of the
connected sockets and if found, loops through the bound sockets and tries
to receive on each of them. Any successfully received messages are checked for
sender ip, and if found to be originating from another router, added to a
message queue to be processed. The logic of the receiver thread is intentionally
designed to be as simple as possible to allow for messages to be continuously
received and queued with minimal blocking.

### Processor Thread

The processor is the most complex of the worker threads, and correspondingly
the least latency dependent. It sleeps until the message queue is non-empty,
dequeues a message, checks whether the message is a HELLO or DV message and
processes accordingly. This is the primary thread that maintains the internal
routing table and neighbor table, while also checking when changes to the
routing table alter the distance vector, and synchronizing the state of the
distance vector with the implemented kernel routes.

When a change in the router's distance vector is detected it is flag to be
sent out as an update by the sender thread and implemented through calls
to `ip route replace ...` or `ip route del`.
