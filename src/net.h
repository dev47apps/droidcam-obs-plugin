#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __WINDOWS__
# include <winsock2.h>
  #define SHUT_RD SD_RECEIVE
  #define SHUT_WR SD_SEND
  #define SHUT_RDWR SD_BOTH
  typedef SOCKET socket_t;
#else
# include <sys/socket.h>
# define INVALID_SOCKET -1
  typedef int socket_t;
#endif

bool net_init(void);
void net_cleanup(void);

socket_t
net_connect(const char* ip, uint16_t port);

ssize_t
net_recv(socket_t socket, void *buf, size_t len);

ssize_t
net_recv_all(socket_t socket, void *buf, size_t len);

ssize_t
net_send(socket_t socket, const void *buf, size_t len);

ssize_t
net_send_all(socket_t socket, const void *buf, size_t len);

bool
net_close(socket_t socket);
#endif
