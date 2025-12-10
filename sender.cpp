#include <cstdint>
#include <pthread.h>
#include <sys/socket.h>

#include "router.h"
#include "sender.h"

void *sender_main(void *arg) {
  sender_data_t *data = (sender_data_t *)arg;
  uint16_t sn = 0;
  while (true) {
    // Check for liveness
    time_t current_time = time(NULL);
    bool dying = false;
    pthread_mutex_lock(data->hello_table->table_mutex);
    hello_entry_t *current_entry = data->hello_table->head;
    while (current_entry != NULL) {
      if (current_entry->last_seen <= current_time + 600) {
        current_entry->alive = false;
        dying = true;
      }
      current_entry = current_entry->next;
    }

    if (dying) {
      data->hello_table->neighbor_dead = true;
    }
    pthread_mutex_unlock(data->hello_table->table_mutex);

    // Send HELLOs
    for (size_t i = 0; i < data->sockets.size(); i++) {
      struct sockaddr_in dest_addr;
      memset(&dest_addr, 0, sizeof(dest_addr));
      dest_addr.sin_family = AF_INET;
      dest_addr.sin_port = htons(PROTOCOL_PORT);

      char *broadcast_addr =
          get_str_from_addr(data->interfaces[i].broadcast_addr);
      inet_pton(AF_INET, broadcast_addr, &dest_addr.sin_addr);
      free(broadcast_addr);

      char *local_ip = get_str_from_addr(data->interfaces[i].addr);
      std::string message = std::string(local_ip);
      message += ":HELLO:";

      uint16_t sn_net_order = htons(sn);

      message.append(reinterpret_cast<const char *>(&sn_net_order),
                     sizeof(sn_net_order));

      ssize_t bytes_sent =
          sendto(data->sockets[i].fd, message.data(), message.size(), 0,
                 (struct sockaddr *)&dest_addr, sizeof(dest_addr));

      pthread_mutex_lock(data->cout_mutex);
      std::cout << "Sent HELLO on " << data->interfaces[i].name
                << " (SN: " << sn << ", Bytes: " << bytes_sent << ")"
                << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
    }

    // Send DV Updates
    char *dv_msg;
    pthread_mutex_lock(data->cout_mutex);
    std::cout << "Sender acquiring table lock" << std::endl;
    pthread_mutex_unlock(data->cout_mutex);
    pthread_mutex_lock(data->routing_table->table_mutex);
    pthread_mutex_lock(data->cout_mutex);
    std::cout << "Sender acquired table lock" << std::endl;
    pthread_mutex_unlock(data->cout_mutex);
    if (data->routing_table->update_dv) {
      for (size_t i = 0; i < data->sockets.size(); i++) {
        struct sockaddr_in dest_addr;
        memset(&dest_addr, 0, sizeof(dest_addr));
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(PROTOCOL_PORT);

        char *broadcast_addr =
            get_str_from_addr(data->interfaces[i].broadcast_addr);
        inet_pton(AF_INET, broadcast_addr, &dest_addr.sin_addr);
        free(broadcast_addr);

        dv_msg =
            get_distance_vector(data->routing_table, data->interfaces[i].addr);

        ssize_t bytes_sent =
            sendto(data->sockets[i].fd, dv_msg, strlen(dv_msg), 0,
                   (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        pthread_mutex_lock(data->cout_mutex);
        std::cout << "Sent DV Update on " << data->interfaces[i].name
                  << " (Bytes: " << bytes_sent << ")" << std::endl;
        pthread_mutex_unlock(data->cout_mutex);
      }
    }
    pthread_mutex_unlock(data->routing_table->table_mutex);

    sn++;
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}
