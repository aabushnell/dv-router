#include "receiver.h"
#include <netinet/in.h>
#include <sys/socket.h>

void *receiver_main(void *arg) {
  receiver_data_t *data = (receiver_data_t *)arg;
  fd_set readfds;
  int max_fd = 0;

  char *buffer = (char *)malloc(4096);
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
          int n = recvfrom(s.fd, buffer, sizeof(buffer) - 1, 0,
                           (struct sockaddr *)&sender_addr, &addr_len);
          if (n > 0) {
            buffer[n] = '\0';
            char sender[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, sender, INET_ADDRSTRLEN);

            pthread_mutex_lock(data->cout_mutex);
            std::cout << "Received update from " << sender << " on " << s.name
                      << std::endl;
            std::cout << "  :" << buffer << std::endl;
            pthread_mutex_unlock(data->cout_mutex);
          }
        }
      }
    }
  }
}
