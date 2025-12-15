#include <netinet/in.h>
#include <sys/socket.h>

#include "receiver.h"
#include "router.h"

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
    for (int i = 0; i < data->sock_count; i++) {
      router_socket_t s = data->sockets[i];
      FD_SET(s.fd, &readfds);
      if (s.fd > max_fd) {
        max_fd = s.fd;
      }
    }

    // select functionality based on
    // https://www.man7.org/linux/man-pages/man2/select.2.html

    int pending_sockets = select(max_fd + 1, &readfds, NULL, NULL, NULL);

    if (pending_sockets > 0) {
      for (int i = 0; i < data->sock_count; i++) {
        router_socket_t s = data->sockets[i];
        if (FD_ISSET(s.fd, &readfds)) {
          int n = recvfrom(s.fd, buffer, REC_BUFF_SIZE - 1, 0,
                           (struct sockaddr *)&sender_addr, &addr_len);
          if (n > 0) {
            buffer[n] = '\0';
            char sender[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sender_addr.sin_addr, sender, INET_ADDRSTRLEN);

            ip_addr_t sender_ip = get_addr_from_str(sender);

            bool is_local = false;

            for (int j = 0; j < data->local_ip_count; j++) {
              if (addr_cmpr(sender_ip, data->local_ips[j])) {
                is_local = true;
                break;
              }
            }

            // ignore messages from self
            if (is_local) {
              continue;
            }

            msg_queue_entry_t *new_node =
                (msg_queue_entry_t *)malloc(sizeof(*new_node));
            if (!new_node) {
              continue;
            }

            new_node->msg_str = (char *)malloc(n + 1);
            memcpy(new_node->msg_str, buffer, n);
            new_node->msg_str[n] = '\0';

            strncpy(new_node->int_name, data->sockets[i].int_name, 15);
            new_node->int_name[15] = '\0';
            new_node->next = NULL;

            pthread_mutex_lock(data->msg_queue->queue_mutex);

            if (data->msg_queue->head == NULL) {
              data->msg_queue->head = new_node;
            } else {
              data->msg_queue->tail->next = new_node;
            }
            data->msg_queue->tail = new_node;
            data->msg_queue->queue_len++;

            pthread_mutex_unlock(data->msg_queue->queue_mutex);
            pthread_cond_signal(data->msg_queue->queue_cond);

            pthread_mutex_lock(data->cout_mutex);
            printf("Received update from %s on %s\n", sender,
                   data->sockets[i].int_name);
            printf("  : %s\n", buffer);
            pthread_mutex_unlock(data->cout_mutex);
          }
        }
      }
    }
  }

  free(buffer);
}
