#ifndef RECEIVER_H_INCLUDED
#define RECEIVER_H_INCLUDED

#include "router.h"

#define REC_BUFF_SIZE 4096

typedef struct receiver_data_t {
  ip_addr_t *local_ips;
  int local_ip_count;
  router_socket_t *sockets;
  int sock_count;
  msg_queue_t *msg_queue;
  pthread_mutex_t *cout_mutex;
} receiver_data_t;

void *receiver_main(void *arg);

#endif
