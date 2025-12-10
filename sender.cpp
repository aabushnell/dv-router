#include <cstdint>
#include <sys/socket.h>

#include "router.h"
#include "sender.h"

void *sender_main(void *arg) {
  sender_data_t *data = (sender_data_t *)arg;
  uint16_t sn = 0;
  while (true) {
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

    sn++;
    std::this_thread::sleep_for(std::chrono::seconds(5));
  }
}
