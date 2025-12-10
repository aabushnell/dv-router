#ifndef ROUTER_H_INCLUDED
#define ROUTER_H_INCLUDED

#include <arpa/inet.h>
#include <cstring>
#include <ifaddrs.h>
#include <iostream>
#include <net/if.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "network.h"

#ifndef SO_BINDTODEVICE
#define SO_BINDTODEVICE 25
#endif

#define MSG_QUEUE_LEN 10

#define PROTOCOL_PORT 5555

typedef struct router_msg_t {
  router_msg_t *next;
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
  msg_queue_entry_t *next;
  char *msg_str;
} msg_queue_entry_t;

typedef struct msg_queue_t {
  msg_queue_entry_t *head;
  msg_queue_entry_t *tail;
  pthread_mutex_t *queue_mutex;
  pthread_cond_t *queue_cond;
  size_t queue_len;
} msg_queue_t;

typedef struct interface_info_t {
  std::string name;
  ip_addr_t addr;
  ip_addr_t broadcast_addr;
  ip_subnet_t subnet;
} interface_info_t;

typedef struct router_socket_t {
  std::string name;
  int fd;
} router_socket_t;

typedef struct hello_entry_t {
  hello_entry_t *next;

  ip_addr_t ip;
  uint16_t last_sn;
  time_t last_seen;
  bool alive;
  char int_name[16];
} hello_entry_t;

typedef struct hello_table_t {
  hello_entry_t *head;
  pthread_mutex_t *table_mutex;
} hello_table_t;

void *router_main(void *arg);

std::vector<interface_info_t> get_interfaces(pthread_mutex_t *cout_mutex);

std::vector<ip_addr_t> get_local_ips(std::vector<interface_info_t> &interfaces);

std::vector<router_socket_t>
bind_sockets(std::vector<interface_info_t> &interfaces,
             pthread_mutex_t *cout_mutex);

#endif
