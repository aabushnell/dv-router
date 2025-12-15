#ifndef RECEIVER_H_INCLUDED
#define RECEIVER_H_INCLUDED

#include "router.h"

#define REC_BUFF_SIZE 4096

typedef struct receiver_data_t {
  local_ip_list_t local_ips;
  socket_list_t sockets;
  msg_queue_t *msg_queue;
  pthread_mutex_t *cout_mutex;
} receiver_data_t;

void *receiver_main(void *arg);

#endif
