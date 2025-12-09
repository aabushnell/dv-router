#ifndef PROCESSOR_H_INCLUDED
#define PROCESSOR_H_INCLUDED

#include "network.h"
#include "router.h"

typedef struct processor_data_t {
  msg_queue_t *msg_queue;
  dv_table_t *table;
} processor_data_t;

char *get_msg_queue_head(msg_queue_t *queue);

void process_distance_vector(dv_parsed_msg_t *msg, dv_table_t *table);

#endif
