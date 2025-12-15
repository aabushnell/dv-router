#ifndef SENDER_H_INCLUDED
#define SENDER_H_INCLUDED

#include <thread>
#include <vector>

#include "network.h"
#include "router.h"

typedef struct sender_data_t {
  interface_info_t *interfaces;
  router_socket_t *sockets;
  int int_count;
  hello_table_t *hello_table;
  dv_table_t *routing_table;
  pthread_mutex_t *cout_mutex;
} sender_data_t;

void *sender_main(void *arg);

#endif
