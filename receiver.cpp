#include "receiver.h"
#include <netinet/in.h>
#include <sys/socket.h>

void *receiver_main(void *arg) {
  receiver_data_t *data = (receiver_data_t *)arg;
  fd_set readfds;
  int max_fd = 0;

  char *buffer = (char *)malloc(REC_BUFF_SIZE);
  if (!buffer) {
    return NULL;
  }

  struct sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);

  while (true) {
    FD_ZERO(&readfds);
    for (auto &s : data->sockets) {
      FD_SET(s.fd, &readfds);
      if (s.fd > max_fd) {
        max_fd = s.fd;
      }
    }

    int pending_sockets = select(max_fd + 1, &readfds, NULL, NULL, NULL);

    if (pending_sockets > 0) {
      for (auto &s : data->sockets) {
        if (FD_ISSET(s.fd, &readfds)) {
          int n = recvfrom(s.fd, buffer, REC_BUFF_SIZE - 1, 0,
                           (struct sockaddr *)&sender_addr, &addr_len);
          if (n > 0) {
            buffer[n] = '\0';
            char sender[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, sender, INET_ADDRSTRLEN);

            ip_addr_t sender_ip = get_addr_from_str(sender);

            bool is_local = false;

            for (auto &local_ip : data->local_ips) {
              if (addr_cmpr(sender_ip, local_ip) == 0) {
                is_local = true;
                break;
              }
            }

            // ignore messages from self
            if (is_local) {
              continue;
            }

            pthread_mutex_lock(data->cout_mutex);
            std::cout << "Received update from " << sender << " on " << s.name
                      << std::endl;
            std::cout << "  : " << buffer << std::endl;
            pthread_mutex_unlock(data->cout_mutex);
          }
        }
      }
    }
  }

  free(buffer);
}
