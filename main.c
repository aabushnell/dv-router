#include <pthread.h>
#include <stdlib.h>

#include "router.h"

int main(void) {
  pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;

  router_data_t data = {&cout_mutex, 0};

  router_main(&data);

  pthread_mutex_destroy(&cout_mutex);

  return EXIT_SUCCESS;
}
