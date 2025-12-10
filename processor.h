#ifndef PROCESSOR_H_INCLUDED
#define PROCESSOR_H_INCLUDED

#include "network.h"
#include "router.h"

typedef struct processor_data_t {
  msg_queue_t *msg_queue;
  hello_table_t *hello_table;
  dv_table_t *table;
  pthread_mutex_t *cout_mutex;
} processor_data_t;

msg_queue_entry_t *get_msg_queue_head(msg_queue_t *queue);

void process_topology_change(hello_table_t *hello_table,
                             dv_table_t *routing_table);

void process_hello(char *msg, char *int_name, hello_table_t *hello_table,
                   pthread_mutex_t *cout_mutex);

void process_distance_vector(dv_parsed_msg_t *msg, dv_table_t *table);

void *processor_main(void *arg);

#endif
