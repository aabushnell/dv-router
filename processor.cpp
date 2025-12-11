#include "processor.h"
#include "network.h"
#include "router.h"
#include <pthread.h>

void *processor_main(void *arg) {
  processor_data_t *data = (processor_data_t *)arg;

  while (true) {
    // Check for changes in immediate topology
    pthread_mutex_lock(data->hello_table->table_mutex);
    bool added = data->hello_table->neighbor_added;
    bool dead = data->hello_table->neighbor_dead;
    pthread_mutex_unlock(data->hello_table->table_mutex);

    if ((added || dead) && false) {
      pthread_mutex_lock(data->cout_mutex);
      std::cout << "Processing topology change" << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
      process_topology_change(data->hello_table, data->table);
      print_routing_table(data->table, data->cout_mutex);
    }

    // Check message queue
    pthread_mutex_lock(data->msg_queue->queue_mutex);
    while (data->msg_queue->queue_len == 0) {
      pthread_cond_wait(data->msg_queue->queue_cond,
                        data->msg_queue->queue_mutex);
    }

    msg_queue_entry_t *msg_entry = get_msg_queue_head(data->msg_queue);
    pthread_mutex_unlock(data->msg_queue->queue_mutex);

    if (msg_entry == NULL) {
      continue;
    }

    msg_type_t type = get_msg_type(msg_entry->msg_str);

    if (type == MSG_HELLO) {
      pthread_mutex_lock(data->cout_mutex);
      // std::cout << "Processing msg of type MSG_HELLO" << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
      process_hello(msg_entry->msg_str, msg_entry->int_name, data->hello_table,
                    data->cout_mutex);
      continue;
    }

    if (type == MSG_DV) {
      pthread_mutex_lock(data->cout_mutex);
      // std::cout << "Processing msg of type MSG_DV: " << msg_entry->msg_str
      //           << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
      dv_parsed_msg_t *msg =
          parse_distance_vector(msg_entry->msg_str, data->cout_mutex);
      if (msg) {
        process_distance_vector(msg, data->table, data->cout_mutex);
      } else {
        pthread_mutex_lock(data->cout_mutex);
        std::cout << "ERROR: Could not parse message" << std::endl;
        pthread_mutex_unlock(data->cout_mutex);
      }
      print_routing_table(data->table, data->cout_mutex);
      continue;
    }

    free(msg_entry->msg_str);
    free(msg_entry);

    pthread_mutex_lock(data->cout_mutex);
    std::cout << "Processing msg of type MSG_UNKNOWN" << std::endl;
    pthread_mutex_unlock(data->cout_mutex);
  }
}

msg_queue_entry_t *get_msg_queue_head(msg_queue_t *queue) {
  msg_queue_entry_t *head = queue->head;
  if (head->next != NULL) {
    queue->head = head->next;
    queue->queue_len--;
  } else {
    queue->head = NULL;
    queue->tail = NULL;
    queue->queue_len = 0;
  }
  return head;
}

void process_topology_change(hello_table_t *hello_table,
                             dv_table_t *routing_table) {
  bool dv_updated = false;

  pthread_mutex_lock(hello_table->table_mutex);
  pthread_mutex_lock(routing_table->table_mutex);

  hello_entry_t *current_entry = hello_table->head;

  while (current_entry != NULL) {
    ip_subnet_t link_subnet;
    link_subnet.addr = current_entry->ip;
    link_subnet.prefix_len = 24;
    link_subnet.addr.f4 = 0;

    dv_dest_entry_t *dest = routing_table->head;
    while (dest != NULL) {
      if (subnet_cmpr(dest->dest, link_subnet)) {
        break;
      }
      dest = dest->next;
    }

    if (dest == NULL) {
      dest = (dv_dest_entry_t *)malloc(sizeof(*dest));
      dest->dest = link_subnet;
      dest->head = NULL;
      dest->best = NULL;
      dest->best_cost = INFINITY_COST;
      dest->next = routing_table->head;
      routing_table->head = dest;
    }

    dv_neighbor_entry_t *route = dest->head;
    while (route != NULL) {
      if (addr_cmpr(route->neighbor_addr, current_entry->ip)) {
        break;
      }
      route = route->next;
    }

    if (route == NULL) {
      route = (dv_neighbor_entry_t *)malloc(sizeof(*route));
      route->neighbor_addr = current_entry->ip;
      route->next = dest->head;
      dest->head = route;
    }

    uint32_t new_cost = current_entry->alive ? 1 : INFINITY_COST;

    if (route->cost != new_cost) {
      route->cost = new_cost;

      dv_neighbor_entry_t *scan = dest->head;
      uint32_t min_cost = INFINITY_COST;
      dv_neighbor_entry_t *best = NULL;

      while (scan != NULL) {
        if (scan->cost < min_cost) {
          min_cost = scan->cost;
          best = scan;
        }
        scan = scan->next;
      }
      if (dest->best != best || dest->best_cost != min_cost) {
        dest->best_cost = min_cost;
        dest->best = best;
        dv_updated = true;
      }
    }
    current_entry = current_entry->next;
  }

  if (dv_updated) {
    dv_update(routing_table);
  }

  pthread_mutex_unlock(hello_table->table_mutex);
  pthread_mutex_unlock(routing_table->table_mutex);
}

void process_hello(char *msg, char *int_name, hello_table_t *hello_table,
                   pthread_mutex_t *cout_mutex) {
  char *first_colon = strchr(msg, ':');
  if (!first_colon) {
    return;
  }

  *first_colon = '\0';
  ip_addr_t sender_ip = get_addr_from_str(msg);
  *first_colon = ':';

  char *hello_ptr = strstr(msg, "HELLO");
  if (!hello_ptr) {
    return;
  }

  uint16_t sn_net;
  memcpy(&sn_net, hello_ptr + 7, sizeof(sn_net));
  uint16_t sn = ntohs(sn_net);

  pthread_mutex_lock(hello_table->table_mutex);

  hello_entry_t *current_entry = hello_table->head;

  bool match_found = false;
  while (current_entry != NULL) {
    if (addr_cmpr(current_entry->ip, sender_ip)) {
      match_found = true;
      if (current_entry->last_sn < sn) {
        current_entry->last_sn = sn;
        current_entry->last_seen = time(NULL);
        current_entry->alive = true;
      }
      break;
    }
    current_entry = current_entry->next;
  }

  if (!match_found) {
    hello_entry_t *new_entry = (hello_entry_t *)malloc(sizeof(*new_entry));
    new_entry->ip = sender_ip;
    new_entry->last_sn = sn;
    new_entry->last_seen = time(NULL);
    new_entry->alive = true;
    strcpy(new_entry->int_name, int_name);

    new_entry->next = hello_table->head;
    hello_table->head = new_entry;

    hello_table->neighbor_added = true;

    pthread_mutex_lock(cout_mutex);
    char *sender_ip_str = get_str_from_addr(sender_ip);
    std::cout << "New Neighbor Found @ " << sender_ip_str << "!" << std::endl;
    pthread_mutex_unlock(cout_mutex);
  }

  pthread_mutex_unlock(hello_table->table_mutex);
}

void process_distance_vector(dv_parsed_msg_t *msg, dv_table_t *table,
                             pthread_mutex_t *cout_mutex) {
  bool dv_updated = false;

  pthread_mutex_lock(table->table_mutex);

  dv_parsed_entry_t *current_route = msg->head;

  if (current_route == NULL) {
    pthread_mutex_lock(cout_mutex);
    std::cout << "ERROR: parsed message malformed" << std::endl;
    pthread_mutex_unlock(cout_mutex);
  }

  while (current_route != NULL) {
    pthread_mutex_lock(cout_mutex);
    char *crd = get_str_from_subnet(current_route->dest);
    // std::cout << "~~ Parsing route " << crd << std::endl;
    free(crd);
    pthread_mutex_unlock(cout_mutex);

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
      dest->best_cost = INFINITY_COST;
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
      current_neighbor = current_neighbor->next;
    }

    // create new entry for neighbor if needed
    if (neighbor == NULL) {
      neighbor = (dv_neighbor_entry_t *)malloc(sizeof(*neighbor));
      neighbor->neighbor_addr = msg->sender;
      neighbor->cost = INFINITY_COST;
      neighbor->next = dest->head;
      dest->head = neighbor;
    }

    pthread_mutex_lock(cout_mutex);
    char *nba = get_str_from_addr(neighbor->neighbor_addr);
    // std::cout << "~~ Parsing neighbor " << nba
    //           << " with current cost: " << neighbor->cost
    //           << " and new cost: " << current_route->cost + 1 << std::endl;
    free(nba);
    pthread_mutex_unlock(cout_mutex);

    uint32_t new_cost = current_route->cost + 1;
    if (new_cost > INFINITY_COST) {
      new_cost = INFINITY_COST;
    }

    bool is_best = (dest->best == neighbor);
    uint32_t old_cost = neighbor->cost;
    neighbor->cost = new_cost;

    if (new_cost < dest->best_cost) {
      // if new cost is better than old best cost
      dest->best_cost = new_cost;
      dest->best = neighbor;
      dv_updated = true;
    } else if (is_best && new_cost > old_cost) {
      // if new cost is worse and this was previously the best route
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
    current_route = current_route->next;
  }
  if (dv_updated) {
    pthread_mutex_lock(cout_mutex);
    std::cout << "DV Updated!" << std::endl;
    pthread_mutex_unlock(cout_mutex);
    dv_update(table);
  }
  pthread_mutex_unlock(table->table_mutex);
}
