#include <cstdlib>
#include <iostream>
#include <pthread.h>

#include "router.h"

int main(void) {
  std::cout << "Hello routers!" << std::endl;

  pthread_mutex_t cout_mutex = PTHREAD_MUTEX_INITIALIZER;

  router_data_t data = {&cout_mutex, 0};

  router_main(&data);

  pthread_mutex_destroy(&cout_mutex);

  return EXIT_SUCCESS;
}
