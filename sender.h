#ifndef SENDER_H_INCLUDED
#define SENDER_H_INCLUDED

#include <thread>
#include <vector>

#include "network.h"
#include "router.h"

typedef struct sender_data_t {
  interface_list_t interfaces;
  socket_list_t sockets;
  hello_table_t *hello_table;
  dv_table_t *routing_table;
  pthread_mutex_t *cout_mutex;
} sender_data_t;

void *sender_main(void *arg);

#endif
