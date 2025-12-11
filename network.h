#ifndef NETWORK_H_INCLUDED
#define NETWORK_H_INCLUDED

#include <arpa/inet.h>
#include <cstdint>
#include <ifaddrs.h>
#include <iomanip>
#include <iostream>
#include <net/if.h>
#include <pthread.h>

#define INFINITY_COST 16

typedef struct ip_addr_t {
  uint8_t f1;
  uint8_t f2;
  uint8_t f3;
  uint8_t f4;
} ip_addr_t;

typedef struct ip_subnet_t {
  ip_addr_t addr;
  uint8_t prefix_len;
} ip_subnet_t;

// linked list of advertised neighbor
// routes for a specific destination
typedef struct dv_neighbor_entry_t {
  dv_neighbor_entry_t *next;

  ip_addr_t neighbor_addr;
  uint32_t cost;
} dv_neighbor_entry_t;

// linked list of advertised destinations
// containing dv_neighbor_route list
typedef struct dv_dest_entry_t {
  dv_dest_entry_t *next;

  ip_subnet_t dest;
  dv_neighbor_entry_t *head;
  dv_neighbor_entry_t *best;
  uint32_t best_cost;
} dv_dest_entry_t;

// wrapper struct for head of ll
typedef struct dv_table_t {
  dv_dest_entry_t *head;
  pthread_mutex_t *table_mutex;
  bool update_dv;
} dv_table_t;

typedef struct dv_parsed_entry_t {
  dv_parsed_entry_t *next;

  ip_subnet_t dest;
  uint32_t cost;
} dv_parsed_entry_t;

typedef struct dv_parsed_msg_t {
  ip_addr_t sender;
  dv_parsed_entry_t *head;
} dv_parsed_msg_t;

typedef enum { MSG_UNKOWN, MSG_HELLO, MSG_DV } msg_type_t;

ip_addr_t get_addr_from_str(char *str);

char *get_str_from_addr(ip_addr_t addr);

bool addr_cmpr(ip_addr_t addr1, ip_addr_t addr2);

ip_subnet_t get_subnet_from_str(char *str);

char *get_str_from_subnet(ip_subnet_t);

bool subnet_cmpr(ip_subnet_t subnet1, ip_subnet_t subnet2);

int netmask_to_prefix(char *netmask_str);

char *get_distance_vector(dv_table_t *table, ip_addr_t sender);

dv_parsed_msg_t *parse_distance_vector(char *dv_str,
                                       pthread_mutex_t *cout_mutex);

void free_parsed_msg(dv_parsed_msg_t *msg);

msg_type_t get_msg_type(char *msg);

void add_direct_route(dv_table_t *table, ip_subnet_t subnet, uint32_t cost,
                      pthread_mutex_t *cout_mutex);

void dv_update(dv_table_t *table);

void dv_sent(dv_table_t *table);

void print_dv_table(dv_table_t *table, pthread_mutex_t *cout_mutex);

void print_routing_table(dv_table_t *table, pthread_mutex_t *cout_mutex);

#endif
