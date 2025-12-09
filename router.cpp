#include "router.h"
#include <cstdlib>

pthread_mutex_t cout_mutex;

void *router_main(void *arg) {
  router_data_t *data = (router_data_t *)arg;

  int router_id = data->router_id;
  int router_count = data->router_count;

  router_msg_box_t *msg_box = data->msg_boxes[router_id];

  pthread_mutex_lock(data->cout_mutex);
  std::cout << "Hello I am router " << router_id << std::endl;
  for (int i = 0; i < router_count; i++) {
    if (data->adj_matrix[router_id * router_count + i] > 0) {
      std::cout << "  and I can see router " << i << std::endl;
    }
  }
  pthread_mutex_unlock(data->cout_mutex);

  pthread_t msg_sender;
  pthread_t msg_receiver;
  pthread_t msg_processor;

  return EXIT_SUCCESS;
}

void broadcast(std::string message, router_data_t *data) {
  for (int i = 0; i < data->router_count; i++) {
    if (data->adj_matrix[data->router_id * data->router_count + i] > 0) {
      std::cout << "  send message to " << i << std::endl;
    }
  }
}
