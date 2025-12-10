#include "processor.h"
#include "network.h"
#include "router.h"

void *processor_main(void *arg) {
  processor_data_t *data = (processor_data_t *)arg;

  while (true) {
    char *msg_str = NULL;
    pthread_mutex_lock(data->msg_queue->queue_mutex);
    while (data->msg_queue->queue_len == 0) {
      pthread_cond_wait(data->msg_queue->queue_cond,
                        data->msg_queue->queue_mutex);
    }

    msg_str = get_msg_queue_head(data->msg_queue);
    pthread_mutex_unlock(data->msg_queue->queue_mutex);

    if (msg_str == NULL) {
      continue;
    }

    msg_type_t type = get_msg_type(msg_str);

    if (type == MSG_HELLO) {
      pthread_mutex_lock(data->cout_mutex);
      std::cout << "Processing msg of type MSG_HELLO" << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
      // process_hello();
      continue;
    }

    if (type == MSG_DV) {
      pthread_mutex_lock(data->cout_mutex);
      std::cout << "Processing msg of type MSG_DV" << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
      dv_parsed_msg_t *msg = parse_distance_vector(msg_str);
      if (msg) {
        process_distance_vector(msg, data->table);
      }
      continue;
    }

    pthread_mutex_lock(data->cout_mutex);
    std::cout << "Processing msg of type MSG_UNKNOWN" << std::endl;
    pthread_mutex_unlock(data->cout_mutex);
  }
}

char *get_msg_queue_head(msg_queue_t *queue) {
  msg_queue_entry_t *head = queue->head;
  if (head->next != NULL) {
    queue->head = head->next;
    queue->queue_len--;
  } else {
    queue->head = NULL;
    queue->queue_len = 0;
  }
  char *msg_str = head->msg_str;
  free(head);
  return msg_str;
}

void process_distance_vector(dv_parsed_msg_t *msg, dv_table_t *table) {
  bool dv_updated = false;

  pthread_mutex_lock(table->table_mutex);

  dv_parsed_entry_t *current_route = msg->head;

  while (current_route != NULL) {

    dv_dest_entry_t *current_entry = table->head;
    dv_dest_entry_t *dest = NULL;

    // search for dest in current routing table
    while (current_entry != NULL) {
      if (subnet_cmpr(current_route->dest, current_entry->dest)) {
        dest = current_entry;
        break;
      }
      current_entry = current_entry->next;
    }

    // create new entry for dest if needed
    if (dest == NULL) {
      dest = (dv_dest_entry_t *)malloc(sizeof(*dest));
      dest->dest = current_route->dest;
      dest->head = NULL;
      dest->best = NULL;
      dest->best_cost = 16;
      dest->next = table->head;
      table->head = dest;
    }

    dv_neighbor_entry_t *current_neighbor = dest->head;
    dv_neighbor_entry_t *neighbor = NULL;

    // search for neighbor in dest entry
    while (current_neighbor != NULL) {
      if (addr_cmpr(msg->sender, current_neighbor->neighbor_addr)) {
        neighbor = current_neighbor;
        break;
      }
    }

    // create new entry for neighbor if needed
    if (neighbor == NULL) {
      neighbor = (dv_neighbor_entry_t *)malloc(sizeof(*neighbor));
      neighbor->neighbor_addr = msg->sender;
      neighbor->next = dest->head;
      dest->head = neighbor;
    }

    uint32_t new_cost = current_route->cost + 1;
    if (new_cost > INFINITY_COST) {
      new_cost = INFINITY_COST;
    }

    bool is_best = (dest->best == neighbor);
    uint32_t old_cost = neighbor->cost;
    neighbor->cost = new_cost;

    if (new_cost < dest->best_cost) {
      // if new cost is better than old best cost
      // update best cost
      dest->best_cost = new_cost;
      dest->best = neighbor;
      dv_updated = true;
    } else if (is_best && new_cost > old_cost) {
      // if new cost is worse and this was
      // previously the best route, recalculate
      uint32_t min_cost = INFINITY_COST;
      dv_neighbor_entry_t *best_route = NULL;
      dv_neighbor_entry_t *current_cand = dest->head;

      while (current_cand != NULL) {
        if (current_cand->cost < min_cost) {
          best_route = current_cand;
          min_cost = current_cand->cost;
        }
        current_cand = current_cand->next;
      }

      dest->best = best_route;
      dest->best_cost = min_cost;
      dv_updated = true;
    }
  }
  if (dv_updated) {
    dv_update(table);
  }
  pthread_mutex_unlock(table->table_mutex);
}
