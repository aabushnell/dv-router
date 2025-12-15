#include <cstdint>
#include <pthread.h>
#include <sys/socket.h>

#include "router.h"
#include "sender.h"

void *sender_main(void *arg) {
  sender_data_t *data = (sender_data_t *)arg;
  uint16_t sn = 0;
  uint16_t dv_counter = 0;

  while (true) {
    // Check for liveness
    bool dying = false;

    pthread_mutex_lock(data->hello_table->table_mutex);
    hello_entry_t *current_entry = data->hello_table->head;

    while (current_entry != NULL) {
      time_t current_time = time(NULL);
      double age_seconds = difftime(current_time, current_entry->last_seen);

      if (age_seconds > 10 && current_entry->alive) {
        current_entry->alive = false;
        dying = true;
        pthread_mutex_lock(data->cout_mutex);
        printf("Link %s is dead\n", current_entry->int_name);
        pthread_mutex_unlock(data->cout_mutex);
      }
      current_entry = current_entry->next;
    }
    if (dying) {
      data->hello_table->neighbor_dead = true;
    }
    pthread_mutex_unlock(data->hello_table->table_mutex);

    print_hello_table(data->hello_table, data->cout_mutex);

    // Send HELLOs
    for (size_t i = 0; i < data->int_count; i++) {
      struct sockaddr_in dest_addr;
      memset(&dest_addr, 0, sizeof(dest_addr));
      dest_addr.sin_family = AF_INET;
      dest_addr.sin_port = htons(PROTOCOL_PORT);

      char *broadcast_addr =
          get_str_from_addr(data->interfaces[i].broadcast_addr);
      inet_pton(AF_INET, broadcast_addr, &dest_addr.sin_addr);
      free(broadcast_addr);

      char *local_ip = get_str_from_addr(data->interfaces[i].addr);

      size_t ip_len = strlen(local_ip);
      size_t msg_len = ip_len + 7 + sizeof(uint16_t);

      char *message = (char *)malloc(msg_len);
      memcpy(message, local_ip, ip_len);
      memcpy(message + ip_len, ":HELLO:", 7);
      uint16_t sn_net_order = htons(sn);
      memcpy(message + ip_len + 7, &sn_net_order, sizeof(sn_net_order));

      ssize_t bytes_sent =
          sendto(data->sockets[i].fd, message, msg_len, 0,
                 (struct sockaddr *)&dest_addr, sizeof(dest_addr));

      pthread_mutex_lock(data->cout_mutex);
      printf("Sent HELLO on %s (SN: %d, Bytes: %ld)\n",
             data->interfaces[i].int_name, sn, bytes_sent);
      pthread_mutex_unlock(data->cout_mutex);
    }

    // Send DV Updates
    pthread_mutex_lock(data->routing_table->table_mutex);
    if (data->routing_table->update_dv || dv_counter > 4) {
      for (int i = 0; i < data->int_count; i++) {
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PROTOCOL_PORT);

        char *broadcast_addr =
            get_str_from_addr(data->interfaces[i].broadcast_addr);
        inet_pton(AF_INET, broadcast_addr, &dest_addr.sin_addr);
        free(broadcast_addr);

        char *dv_msg =
            get_distance_vector(data->routing_table, data->interfaces[i].addr);

        ssize_t bytes_sent =
            sendto(data->sockets[i].fd, dv_msg, strlen(dv_msg), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr));

        pthread_mutex_lock(data->cout_mutex);
        printf("Sent DV Update on %s (Bytes: %ld)\n",
               data->interfaces[i].int_name, bytes_sent);
        pthread_mutex_unlock(data->cout_mutex);
        free(dv_msg);
      }
      dv_sent(data->routing_table);
      dv_counter = 0;
    }
    pthread_mutex_unlock(data->routing_table->table_mutex);

    sn++;
    dv_counter++;
    sleep(5);
  }
}
