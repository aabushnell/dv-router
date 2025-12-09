#include <cstdlib>
#include <iostream>
#include <pthread.h>

#include "router.h"

#define THREAD_COUNT 4

// clang-format off
char adj_matrix[THREAD_COUNT * THREAD_COUNT] = {
      0, 1, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 1,
      0, 0, 0, 0
};
// clang-format on

int main(void) {
  std::cout << "Hello routers!" << std::endl;

  pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;

  pthread_t threads[THREAD_COUNT];
  router_data_t router_data[THREAD_COUNT];

  router_msg_box_t *msg_boxes[THREAD_COUNT];

  for (int t = 0; t < THREAD_COUNT; t++) {
    msg_boxes[t] = NULL;
  }

  for (int t = 0; t < THREAD_COUNT; t++) {
    router_data[t].msg_boxes = msg_boxes;
    router_data[t].adj_matrix = adj_matrix;
    router_data[t].cout_mutex = &cout_mutex;
    router_data[t].router_count = THREAD_COUNT;
    router_data[t].router_id = t;
  }

  for (int t = 0; t < THREAD_COUNT; t++) {
    pthread_create(&threads[t], NULL, router_main, (void *)&router_data[t]);
  }

  for (int t = 0; t < THREAD_COUNT; t++) {
    pthread_join(threads[t], NULL);
  }

  pthread_mutex_destroy(&cout_mutex);

  return EXIT_SUCCESS;
}
