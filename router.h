#ifndef ROUTER_H_INCLUDED
#define ROUTER_H_INCLUDED

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "network.h"

#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE 25
#endif

#define MSG_QUEUE_LEN 10

#define PROTOCOL_PORT 5555

typedef struct router_msg_t {
  struct router_msg_t *next;
  char *msg;
} router_msg_t;

typedef struct router_msg_box_t {
  router_msg_t *head;
  int max_count;
  int unread_count;
} router_msg_box_t;

typedef struct router_data_t {
  pthread_mutex_t *cout_mutex;
  int router_id;
} router_data_t;

typedef struct msg_queue_entry_t {
  struct msg_queue_entry_t *next;
  char *msg_str;
  char int_name[16];
} msg_queue_entry_t;

typedef struct msg_queue_t {
  msg_queue_entry_t *head;
  msg_queue_entry_t *tail;
  pthread_mutex_t *queue_mutex;
  pthread_cond_t *queue_cond;
  size_t queue_len;
} msg_queue_t;

typedef struct interface_info_t {
  char int_name[16];
  ip_addr_t addr;
  ip_addr_t broadcast_addr;
  ip_subnet_t subnet;
} interface_info_t;

typedef struct router_socket_t {
  char int_name[16];
  int fd;
} router_socket_t;

typedef struct hello_entry_t {
  struct hello_entry_t *next;

  ip_addr_t ip;
  uint16_t last_sn;
  time_t last_seen;
  bool alive;
  char int_name[16];
} hello_entry_t;

typedef struct hello_table_t {
  hello_entry_t *head;
  pthread_mutex_t *table_mutex;
  bool neighbor_added;
  bool neighbor_dead;
} hello_table_t;

void *router_main(void *arg);

interface_info_t *get_interfaces(pthread_mutex_t *cout_mutex, int *count);

ip_addr_t *get_local_ips(interface_info_t *interfaces, int count,
                         int *out_count);

router_socket_t *bind_sockets(interface_info_t *interfaces, int count,
                              pthread_mutex_t *cout_mutex, int *out_count);

void print_hello_table(hello_table_t *table, pthread_mutex_t *cout_mutex);

void sync_kernel_routes(dv_table_t *table, pthread_mutex_t *cout_mutex);

#endif
