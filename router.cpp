#include <cstdint>
#include <cstdlib>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "network.h"
#include "processor.h"
#include "receiver.h"
#include "router.h"
#include "sender.h"

void *router_main(void *arg) {
  router_data_t *data = (router_data_t *)arg;

  int router_id = data->router_id;

  pthread_mutex_lock(data->cout_mutex);
  std::cout << "Hello I am router " << router_id << std::endl;
  pthread_mutex_unlock(data->cout_mutex);

  interface_list_t interfaces = get_interfaces(data->cout_mutex);
  local_ip_list_t local_ips = get_local_ips(interfaces);
  socket_list_t sockets = bind_sockets(interfaces, data->cout_mutex);

  pthread_mutex_t routing_table_mutex = PTHREAD_MUTEX_INITIALIZER;
  dv_table_t *routing_table = (dv_table_t *)malloc(sizeof(*routing_table));
  routing_table->head = NULL;
  routing_table->table_mutex = &routing_table_mutex;
  routing_table->update_dv = false;

  pthread_mutex_t hello_table_mutex = PTHREAD_MUTEX_INITIALIZER;
  hello_table_t *hello_table = (hello_table_t *)malloc(sizeof(*hello_table));
  hello_table->head = NULL;
  hello_table->table_mutex = &hello_table_mutex;
  hello_table->neighbor_added = false;
  hello_table->neighbor_dead = false;

  pthread_mutex_t msg_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t msg_queue_cond = PTHREAD_COND_INITIALIZER;
  msg_queue_t *msg_queue = (msg_queue_t *)malloc(sizeof(*msg_queue));
  msg_queue->head = NULL;
  msg_queue->tail = NULL;
  msg_queue->queue_mutex = &msg_queue_mutex;
  msg_queue->queue_cond = &msg_queue_cond;
  msg_queue->queue_len = 0;

  pthread_mutex_lock(&routing_table_mutex);

  for (uint16_t i = 0; i < interfaces.count; i++) {
    interface_info_t iface = interfaces.interfaces[i];
    ip_subnet_t connected_net;
    connected_net.addr = iface.addr;
    connected_net.addr.f4 = 0;
    connected_net.prefix_len = iface.subnet.prefix_len;
    add_direct_route(routing_table, connected_net, 1, data->cout_mutex);
    routing_table->update_dv = true;
  }
  pthread_mutex_unlock(&routing_table_mutex);

  print_routing_table(routing_table, data->cout_mutex);

  pthread_t msg_sender;
  sender_data_t sender_data = {interfaces, sockets, hello_table, routing_table,
                               data->cout_mutex};

  pthread_t msg_receiver;
  receiver_data_t receiver_data = {local_ips, sockets, msg_queue,
                                   data->cout_mutex};
  pthread_t msg_processor;
  processor_data_t processor_data = {msg_queue, hello_table, routing_table,
                                     data->cout_mutex};

  pthread_create(&msg_sender, NULL, sender_main, (void *)&sender_data);
  pthread_create(&msg_receiver, NULL, receiver_main, (void *)&receiver_data);
  pthread_create(&msg_processor, NULL, processor_main, (void *)&processor_data);

  while (true) {
    // Check for changes in immediate topology
    pthread_mutex_lock(hello_table->table_mutex);
    // bool added = data->hello_table->neighbor_added;
    bool dead = hello_table->neighbor_dead;
    pthread_mutex_unlock(hello_table->table_mutex);

    if (dead) {
      pthread_mutex_lock(data->cout_mutex);
      std::cout << "Processing topology change" << std::endl;
      pthread_mutex_unlock(data->cout_mutex);
      handle_dead_link(hello_table, routing_table);
      print_routing_table(routing_table, data->cout_mutex);
      pthread_mutex_lock(hello_table->table_mutex);
      hello_table->neighbor_dead = false;
      pthread_mutex_unlock(hello_table->table_mutex);
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
  }

  pthread_join(msg_sender, NULL);
  pthread_join(msg_receiver, NULL);
  pthread_join(msg_processor, NULL);

  return EXIT_SUCCESS;
}

interface_list_t get_interfaces(pthread_mutex_t *cout_mutex) {
  struct ifaddrs *ifaddr, *ifa;

  // implementation of getiaddrs loop inspired by
  // https://www.man7.org/linux/man-pages/man3/getifaddrs.3.html

  if (getifaddrs(&ifaddr) == -1) {
    pthread_mutex_lock(cout_mutex);
    std::cout << "ERROR: getifaddrs failed" << std::endl;
    pthread_mutex_unlock(cout_mutex);
    exit(EXIT_FAILURE);
  }

  uint16_t int_count = 0;

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }
    int_count++;
  }

  interface_info_t *interfaces =
      (interface_info_t *)malloc(int_count * sizeof(*interfaces));

  uint16_t i = 0;
  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == NULL) {
      continue;
    }

    if (ifa->ifa_addr->sa_family == AF_INET) {
      // skip loopback
      if (strcmp(ifa->ifa_name, "lo") == 0) {
        continue;
      }

      interface_info_t info;
      strcpy(info.name, ifa->ifa_name);

      char ip[INET_ADDRSTRLEN];
      void *addr_ptr = &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
      inet_ntop(AF_INET, addr_ptr, ip, INET_ADDRSTRLEN);
      info.addr = get_addr_from_str(ip);

      if (ifa->ifa_flags & IFF_BROADCAST && ifa->ifa_broadaddr) {
        char broadcast[INET_ADDRSTRLEN];
        void *broadcast_ptr =
            &((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_addr;
        inet_ntop(AF_INET, broadcast_ptr, broadcast, INET_ADDRSTRLEN);
        info.broadcast_addr = get_addr_from_str(broadcast);
      }

      if (ifa->ifa_netmask) {
        char mask[INET_ADDRSTRLEN];
        void *mask_ptr = &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr;
        inet_ntop(AF_INET, mask_ptr, mask, INET_ADDRSTRLEN);
        int prefix_len = netmask_to_prefix(mask);

        struct in_addr ip_addr, mask_addr, net_addr;
        inet_pton(AF_INET, ip, &ip_addr);
        inet_pton(AF_INET, mask, &mask_addr);
        net_addr.s_addr = ip_addr.s_addr & mask_addr.s_addr;

        char network[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &net_addr, network, INET_ADDRSTRLEN);

        char subnet[64];
        snprintf(subnet, sizeof(subnet), "%s/%d", network, prefix_len);

        info.subnet = get_subnet_from_str(subnet);
      }

      interfaces[i] = info;
      pthread_mutex_lock(cout_mutex);
      std::cout << "Found interface: " << info.name << std::endl;
      pthread_mutex_unlock(cout_mutex);
      i++;
    }
  }

  freeifaddrs(ifaddr);
  return {interfaces, int_count};
}

local_ip_list_t get_local_ips(interface_list_t interfaces) {
  ip_addr_t *local_ips =
      (ip_addr_t *)malloc(interfaces.count * sizeof(*local_ips));

  for (uint16_t i = 0; i < interfaces.count; i++) {
    local_ips[i] = (interfaces.interfaces[i].addr);
  }

  return {local_ips, interfaces.count};
}

socket_list_t bind_sockets(interface_list_t interfaces,
                           pthread_mutex_t *cout_mutex) {
  router_socket_t *sockets =
      (router_socket_t *)malloc(interfaces.count * sizeof(*sockets));

  for (uint16_t i = 0; i < interfaces.count; i++) {
    interface_info_t iface = interfaces.interfaces[i];
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
      pthread_mutex_lock(cout_mutex);
      std::cout << "ERROR: socket not bound" << std::endl;
      pthread_mutex_unlock(cout_mutex);
      continue;
    }

    int enable_reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable_reuse,
               sizeof(enable_reuse));

    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, iface.name, 16) < 0) {
      pthread_mutex_lock(cout_mutex);
      std::cout << "ERROR: cannot bind to device" << std::endl;
      pthread_mutex_unlock(cout_mutex);
    }

    int enable_broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &enable_broadcast,
               sizeof(enable_broadcast));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PROTOCOL_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (::bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
      pthread_mutex_lock(cout_mutex);
      std::cout << "ERROR: could not bind socket" << std::endl;
      pthread_mutex_unlock(cout_mutex);
      close(sock);
      continue;
    }

    sockets[i] = (router_socket_t){"", sock};
    strcpy(sockets[i].name, iface.name);
    pthread_mutex_lock(cout_mutex);
    std::cout << "Bound socket " << sock << " to " << iface.name << std::endl;
    pthread_mutex_unlock(cout_mutex);
  }

  return {sockets, interfaces.count};
}

void print_hello_table(hello_table_t *table, pthread_mutex_t *cout_mutex) {
  if (!table)
    return;

  pthread_mutex_lock(table->table_mutex);
  pthread_mutex_lock(cout_mutex);

  std::cout << "\n==================== NEIGHBOR TABLE "
               "===========================\n";
  std::cout << "---------------------------------------------------------------"
            << std::endl;
  // clang-format off
  std::cout << std::setw(22) << std::left << "Neighbor"
            << std::setw(16) << std::left << "Interface"
            << std::setw(8) << std::left << "Last SN"
            << std::setw(8) << std::left << "Age"
            << std::setw(8) << std::left << "Status"
            << std::endl;
  // clang-format on
  std::cout << "---------------------------------------------------------------"
            << std::endl;

  hello_entry_t *curr = table->head;
  time_t now = time(NULL);

  while (curr != NULL) {
    char *ip_str = get_str_from_addr(curr->ip);

    // Calculate Age
    double age_seconds = difftime(now, curr->last_seen);

    // Format Status
    std::string status = curr->alive ? "ALIVE" : "DEAD";

    // Ensure interface name is safe to print
    std::string interface =
        (curr->int_name[0] != '\0') ? curr->int_name : "???";

    // clang-format off
    std::cout << std::setw(22) << std::left << ip_str
              << std::setw(16) << std::left << interface
              << std::setw(8) << std::left << curr->last_sn
              << std::setw(8) << std::left << age_seconds
              << std::setw(8) << std::left << status 
              << std::endl;
    // clang-format on

    free(ip_str);
    curr = curr->next;
  }

  if (table->head == NULL) {
    std::cout << "(No neighbors discovered yet)\n";
  }

  std::cout << "---------------------------------------------------------------"
            << std::endl;

  pthread_mutex_unlock(cout_mutex);
  pthread_mutex_unlock(table->table_mutex);
}

void sync_kernel_routes(dv_table_t *table, pthread_mutex_t *cout_mutex) {
  dv_dest_entry_t *dest = table->head;

  while (dest != NULL) {
    if (dest->best != dest->installed) {

      char *dest_str = get_str_from_subnet(dest->dest);

      // New route is valid, old was NULL/different
      if (dest->best != NULL && dest->best_cost < INFINITY_COST) {

        char *gw_ip = get_str_from_addr(dest->best->neighbor_addr);
        char cmd[256];

        // Check if this is a "Direct" route (GW is 0.0.0.0)
        if (!addr_cmpr(dest->best->neighbor_addr, (ip_addr_t){0, 0, 0, 0})) {
          snprintf(cmd, sizeof(cmd), "ip route replace %s via %s", dest_str,
                   gw_ip);
          pthread_mutex_lock(cout_mutex);
          std::cout << "Running command: " << cmd << std::endl;
          pthread_mutex_unlock(cout_mutex);
          system(cmd);
        }
        free(gw_ip);

        dest->installed = dest->best;
      }
      // New route is INVALID (Infinity/NULL), old was valid
      else if (dest->installed != NULL) {
        // Route became unreachable -> Delete it
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "ip route del %s", dest_str);
        pthread_mutex_lock(cout_mutex);
        std::cout << "Running command: " << cmd << std::endl;
        pthread_mutex_unlock(cout_mutex);
        system(cmd);

        dest->installed = NULL;
      }

      free(dest_str);
    }

    dest = dest->next;
  }
}
