#ifndef RECEIVER_H_INCLUDED
#define RECEIVER_H_INCLUDED

#include "router.h"

typedef struct receiver_data_t {
  std::vector<router_socket_t> &sockets;
  pthread_mutex_t *cout_mutex;
} receiver_data_t;

void *receiver_main(void *arg);

#endif
