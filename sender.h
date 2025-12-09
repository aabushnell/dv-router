#ifndef SENDER_H_INCLUDED
#define SENDER_H_INCLUDED

#include "router.h"
#include <thread>
#include <vector>

typedef struct sender_data_t {
  std::vector<interface_info_t> &interfaces;
  std::vector<router_socket_t> &sockets;
  pthread_mutex_t *cout_mutex;
} sender_data_t;

void *sender_main(void *arg);

#endif
