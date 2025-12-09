#include <cstdlib>
#include <netinet/in.h>
#include <sys/socket.h>

#include "network.h"
#include "receiver.h"
#include "router.h"
#include "sender.h"

pthread_mutex_t cout_mutex;

void *router_main(void *arg) {
  router_data_t *data = (router_data_t *)arg;

  int router_id = data->router_id;

  pthread_mutex_lock(data->cout_mutex);
  std::cout << "Hello I am router " << router_id << std::endl;
  pthread_mutex_unlock(data->cout_mutex);

  auto interfaces = get_interfaces(data->cout_mutex);
  auto sockets = bind_sockets(interfaces, data->cout_mutex);

  pthread_t msg_sender;
  sender_data_t sender_data = {interfaces, sockets, data->cout_mutex};

  pthread_t msg_receiver;
  receiver_data_t receiver_data = {sockets, data->cout_mutex};
  pthread_t msg_processor;

  pthread_create(&msg_sender, NULL, sender_main, (void *)&sender_data);
  pthread_create(&msg_receiver, NULL, receiver_main, (void *)&receiver_data);

  pthread_join(msg_sender, NULL);
  pthread_join(msg_receiver, NULL);

  return EXIT_SUCCESS;
}

std::vector<interface_info_t> get_interfaces(pthread_mutex_t *cout_mutex) {
  std::vector<interface_info_t> interfaces;
  struct ifaddrs *ifaddr, *ifa;

  if (getifaddrs(&ifaddr) == -1) {
    pthread_mutex_lock(cout_mutex);
    std::cout << "ERROR: getifaddrs failed" << std::endl;
    pthread_mutex_unlock(cout_mutex);
    exit(EXIT_FAILURE);
  }

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
      info.name = ifa->ifa_name;

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

      interfaces.push_back(info);
      pthread_mutex_lock(cout_mutex);
      std::cout << "Found interface: " << info.name << std::endl;
      pthread_mutex_unlock(cout_mutex);
    }
  }

  freeifaddrs(ifaddr);
  return interfaces;
}

std::vector<router_socket_t>
bind_sockets(std::vector<interface_info_t> &interfaces,
             pthread_mutex_t *cout_mutex) {
  std::vector<router_socket_t> sockets;

  for (auto &iface : interfaces) {
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

    if (setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, iface.name.c_str(),
                   iface.name.length()) < 0) {
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

    sockets.push_back((router_socket_t){iface.name, sock});
    pthread_mutex_lock(cout_mutex);
    std::cout << "Bound socket " << sock << " to " << iface.name << std::endl;
    pthread_mutex_unlock(cout_mutex);
  }

  return sockets;
}
