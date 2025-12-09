#ifndef ROUTER_H_INCLUDED
#define ROUTER_H_INCLUDED

#include <iostream>
#include <pthread.h>

#include "network.h"

#define MSG_QUEUE_LEN 10

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
  router_msg_box_t **msg_boxes;
  char *adj_matrix;
  pthread_mutex_t *cout_mutex;
  int router_count;
  int router_id;
} router_data_t;

typedef struct msg_queue_entry_t {
  msg_queue_entry_t *next;
  char *msg_str;
} msg_queue_entry_t;

typedef struct msg_queue_t {
  msg_queue_entry_t *head;
  pthread_mutex_t *queue_mutex;
  size_t queue_len;
} msg_queue_t;

void *router_main(void *arg);

void broadcast(std::string message, char *adj_matrix);

#endif
