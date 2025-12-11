#include <cstdint>
#include <cstring>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "network.h"

ip_addr_t get_addr_from_str(char *str) {
  ip_addr_t addr = {0, 0, 0, 0};

  if (!str) {
    return addr;
  }

  sscanf(str, "%hhu.%hhu.%hhu.%hhu", &addr.f1, &addr.f2, &addr.f3, &addr.f4);
  return addr;
}

char *get_str_from_addr(ip_addr_t addr) {
  // max str size: 255.255.255.255'\0' ~ 16 chars
  size_t str_size = 16;
  char *str = (char *)malloc(str_size * sizeof(*str));

  snprintf(str, str_size, "%u.%u.%u.%u", addr.f1, addr.f2, addr.f3, addr.f4);
  return str;
}

bool addr_cmpr(ip_addr_t addr1, ip_addr_t addr2) {
  return (addr1.f1 == addr2.f1 && addr1.f2 == addr2.f2 &&
          addr1.f3 == addr2.f3 && addr1.f4 == addr2.f4);
}

ip_subnet_t get_subnet_from_str(char *str) {
  ip_subnet_t subnet = {{0}, 0};

  if (!str) {
    return subnet;
  }

  int n_fields =
      sscanf(str, "%hhu.%hhu.%hhu.%hhu/%hhu", &subnet.addr.f1, &subnet.addr.f2,
             &subnet.addr.f3, &subnet.addr.f4, &subnet.prefix_len);

  if (n_fields == 4) {
    // default to 32 if not specified in str
    subnet.prefix_len = 32;
  }

  return subnet;
}

char *get_str_from_subnet(ip_subnet_t subnet) {
  // max str size: 255.255.255.255/32'\0' ~ 24 chars
  size_t str_size = 24;
  char *str = (char *)malloc(str_size * sizeof(*str));

  snprintf(str, str_size, "%u.%u.%u.%u/%u", subnet.addr.f1, subnet.addr.f2,
           subnet.addr.f3, subnet.addr.f4, subnet.prefix_len);
  return str;
}

bool subnet_cmpr(ip_subnet_t subnet1, ip_subnet_t subnet2) {
  return (addr_cmpr(subnet1.addr, subnet2.addr) &&
          subnet1.prefix_len == subnet2.prefix_len);
}

int netmask_to_prefix(char *netmask_str) {
  struct in_addr addr;
  inet_pton(AF_INET, netmask_str, &addr);
  uint32_t mask = ntohl(addr.s_addr);

  int prefix = 0;
  while (mask & 0x80000000) {
    prefix++;
    mask <<= 1;
  }
  return prefix;
}

char *get_distance_vector(dv_table_t *table, ip_addr_t sender) {
  size_t buffer_len = 128;
  size_t current_len = 0;

  char *buffer = (char *)malloc(buffer_len * sizeof(*buffer));

  char *sender_ip_str = get_str_from_addr(sender);
  size_t header_len = snprintf(buffer, buffer_len, "%s:DV:", sender_ip_str);
  current_len += header_len;
  free(sender_ip_str);

  dv_dest_entry_t *current_entry = table->head;
  // walk dv entry linked list
  while (current_entry != NULL) {
    char *subnet_str = get_str_from_subnet(current_entry->dest);
    size_t entry_len = strlen(subnet_str) + 8;

    if (current_len + entry_len >= buffer_len) {
      buffer_len = buffer_len * 2 + entry_len;
      char *new_buffer = (char *)realloc(buffer, buffer_len);
      buffer = new_buffer;
    }

    size_t char_written =
        snprintf(buffer + current_len, buffer_len - current_len,
                 "(%s,%u):", subnet_str, current_entry->best_cost);

    if (char_written > 0) {
      current_len += char_written;
    }

    current_entry = current_entry->next;
  }

  return buffer;
}

dv_parsed_msg_t *parse_distance_vector(char *dv_str,
                                       pthread_mutex_t *cout_mutex) {
  if (!dv_str) {
    return NULL;
  }
  char *cursor = dv_str;

  dv_parsed_msg_t *msg_ll = (dv_parsed_msg_t *)malloc(sizeof(*msg_ll));
  msg_ll->head = NULL;
  msg_ll->sender = (ip_addr_t){0, 0, 0, 0};

  cursor = strchr(dv_str, ':');
  if (!cursor) {
    free_parsed_msg(msg_ll);
    return NULL;
  }

  size_t sender_len = cursor - dv_str;
  char *sender_ip_buff = (char *)malloc(sender_len + 1);
  memcpy(sender_ip_buff, dv_str, sender_len);
  sender_ip_buff[sender_len] = '\0';

  msg_ll->sender = get_addr_from_str(sender_ip_buff);
  free(sender_ip_buff);

  cursor++;
  if (strncmp(cursor, "DV:", 3) != 0) {
    free_parsed_msg(msg_ll);
    return NULL;
  }
  cursor += 3;

  while (*cursor == '(') {
    pthread_mutex_lock(cout_mutex);
    // std::cout << "~~Parsing new destination" << std::endl;
    pthread_mutex_unlock(cout_mutex);
    cursor++;
    char *close_paren = strchr(cursor, ')');
    char *comma = strchr(cursor, ',');

    size_t subnet_len = comma - cursor;
    char *subnet_buff = (char *)malloc(subnet_len + 1);
    memcpy(subnet_buff, cursor, subnet_len);
    subnet_buff[subnet_len] = '\0';

    ip_subnet_t subnet = get_subnet_from_str(subnet_buff);
    free(subnet_buff);
    cursor = comma + 1;

    uint32_t cost = (uint32_t)strtoul(cursor, NULL, 10);

    dv_parsed_entry_t *entry = (dv_parsed_entry_t *)malloc(sizeof(*entry));
    entry->dest = subnet;
    entry->cost = cost;
    entry->next = msg_ll->head;
    msg_ll->head = entry;

    cursor = close_paren + 2;
  }
  return msg_ll;
}

void free_parsed_msg(dv_parsed_msg_t *msg) {
  if (!msg) {
    return;
  }
  dv_parsed_entry_t *current_entry = msg->head;
  while (current_entry != NULL) {
    dv_parsed_entry_t *next_entry = current_entry->next;
    free(current_entry);
    current_entry = next_entry;
  }
  free(msg);
}

msg_type_t get_msg_type(char *msg) {
  char *first_colon = strchr(msg, ':');
  if (!first_colon) {
    return MSG_UNKOWN;
  }

  if (strncmp(first_colon + 1, "HELLO", 5) == 0) {
    return MSG_HELLO;
  }
  if (strncmp(first_colon + 1, "DV", 2) == 0) {
    return MSG_DV;
  }
  return MSG_UNKOWN;
}

void add_direct_route(dv_table_t *table, ip_subnet_t subnet, uint32_t cost,
                      pthread_mutex_t *cout_mutex) {
  dv_dest_entry_t *current_dest = table->head;
  while (current_dest != NULL) {
    if (subnet_cmpr(current_dest->dest, subnet))
      break;
    current_dest = current_dest->next;
  }

  if (current_dest == NULL) {
    current_dest = (dv_dest_entry_t *)malloc(sizeof(dv_dest_entry_t));
    current_dest->dest = subnet;
    current_dest->head = NULL;
    current_dest->best = NULL; // Will be set below
    current_dest->best_cost = INFINITY_COST;

    // Insert at head
    current_dest->next = table->head;
    table->head = current_dest;

    pthread_mutex_lock(cout_mutex);
    char *subnet_str = get_str_from_subnet(subnet);
    std::cout << "Adding new dest: " << subnet_str << std::endl;
    pthread_mutex_unlock(cout_mutex);
  }

  ip_addr_t direct_gateway = (ip_addr_t){0, 0, 0, 0};

  dv_neighbor_entry_t *current_neighbor = current_dest->head;
  while (current_neighbor != NULL) {
    if (addr_cmpr(current_neighbor->neighbor_addr, direct_gateway))
      break;
    current_neighbor = current_neighbor->next;
  }

  if (current_neighbor == NULL) {
    current_neighbor =
        (dv_neighbor_entry_t *)malloc(sizeof(dv_neighbor_entry_t));
    current_neighbor->neighbor_addr = direct_gateway;

    // Insert at head of neighbors list
    current_neighbor->next = current_dest->head;
    current_dest->head = current_neighbor;
  }

  current_neighbor->cost = cost;

  if (cost < current_dest->best_cost) {
    current_dest->best_cost = cost;
    current_dest->best = current_neighbor;
  }
}

void dv_update(dv_table_t *table) { table->update_dv = true; }

void dv_sent(dv_table_t *table) { table->update_dv = false; }

void print_dv_table(dv_table_t *table, pthread_mutex_t *cout_mutex) {
  if (!table)
    return;

  pthread_mutex_lock(cout_mutex);
  std::cout << "\n=== FORWARDING TABLE ===\n";
  std::cout << "Dest Subnet\t\tGW\t\tCost\n";
  std::cout << "------------------------------------------------------------\n";

  dv_dest_entry_t *dest = table->head;
  while (dest != NULL) {
    char *subnet_str = get_str_from_subnet(dest->dest);

    std::string gw_str = "None";
    std::string cost_str = "INF";

    if (dest->best != NULL) {
      char *gw_ip = get_str_from_addr(dest->best->neighbor_addr);
      gw_str = std::string(gw_ip);
      free(gw_ip);

      cost_str = std::to_string(dest->best_cost);
    } else if (dest->best_cost >= INFINITY_COST) {
      cost_str = "INF";
    }

    std::cout << subnet_str << "\t\t" << gw_str << "\t\t" << cost_str << "\n";

    free(subnet_str);

    dest = dest->next;
  }
  std::cout << "=====================\n\n";
  pthread_mutex_unlock(cout_mutex);
}

void print_routing_table(dv_table_t *table, pthread_mutex_t *cout_mutex) {
  if (!table)
    return;

  pthread_mutex_lock(cout_mutex);
  std::cout << "\n=== ROUTING TABLE ===\n";
  std::cout << "Dest Subnet\t\tGW\t\tCost\tBest?\n";
  std::cout << "---------------------------------------------------------------"
               "-----\n";

  dv_dest_entry_t *dest = table->head;
  while (dest != NULL) {
    char *subnet_str = get_str_from_subnet(dest->dest);

    // Print header for this destination (optional)
    // std::cout << "\n[Destination: " << subnet_str << "]\n";

    // Iterate over ALL neighbors for this destination
    dv_neighbor_entry_t *neigh = dest->head;

    if (neigh == NULL) {
      // No routes for this destination
      std::cout << subnet_str << "\t\t"
                << "---" << "\t\t"
                << "---" << "\t"
                << "---" << "\n";
    }

    while (neigh != NULL) {
      char *gw_ip = get_str_from_addr(neigh->neighbor_addr);

      std::string cost_str =
          (neigh->cost >= INFINITY_COST) ? "INF" : std::to_string(neigh->cost);

      // Mark if this is the best route
      std::string best_marker = (dest->best == neigh) ? " *" : "";

      // clang-format off
      std::cout << std::setw(24) << std::left << subnet_str
                << std::setw(20) << std::left << gw_ip
                << std::setw(8) << std::left << cost_str
                << std::setw(8) << std::left << best_marker
                << std::endl;
      // clang-format on

      free(gw_ip);
      neigh = neigh->next;
    }

    free(subnet_str);
    dest = dest->next;
  }
  std::cout << "============================\n\n";
  pthread_mutex_unlock(cout_mutex);
}
