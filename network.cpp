#include <cstdint>
#include <cstring>
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

char *get_distance_vector(dv_table_t *table, ip_addr_t sender) {
  size_t buffer_len = 128;
  size_t current_len = 0;

  char *buffer = (char *)malloc(buffer_len * sizeof(*buffer));

  char *sender_ip_str = get_str_from_addr(sender);
  size_t header_len = snprintf(buffer, buffer_len, "%s:DV:", sender_ip_str);
  current_len += header_len;
  free(sender_ip_str);

  pthread_mutex_lock(table->mutex);
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
  pthread_mutex_unlock(table->mutex);

  return buffer;
}

dv_parsed_msg_t *parse_distance_vector(char *dv_str) {
  if (!dv_str) {
    return NULL;
  }
  char *cursor = dv_str;

  dv_parsed_msg_t *msg_ll = (dv_parsed_msg_t *)malloc(sizeof(*msg_ll));
  msg_ll->head = NULL;
  msg_ll->sender = (ip_addr_t){0};

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
  cursor += 4;

  while (*cursor == '(') {
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
    entry->next = NULL;
    if (msg_ll->head != NULL) {
      dv_parsed_entry_t *next = msg_ll->head;
      while (next->next != NULL) {
        next = next->next;
      }
      next->next = entry;
    } else {
      msg_ll->head = entry;
    }

    cursor = close_paren + 2;
  }
  return NULL;
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

void dv_update(dv_table_t *table) { table->update_dv = true; }

void dv_sent(dv_table_t *table) { table->update_dv = false; }
