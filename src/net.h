// Copyright (C) 2020 github.com/aramg
#ifndef NET_H
#define NET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
# include <winsock2.h>
  #define SHUT_RD SD_RECEIVE
  #define SHUT_WR SD_SEND
  #define SHUT_RDWR SD_BOTH
  typedef SOCKET socket_t;
  typedef int ssize_t;
#else
# include <sys/socket.h>
# define INVALID_SOCKET -1
  typedef int socket_t;
#endif

bool net_init(void);
void net_cleanup(void);

socket_t
net_connect_and_ping(const char* ip, uint16_t port);

socket_t
net_connect(const char* ip, uint16_t port);

ssize_t
net_recv(socket_t sock, void *buf, size_t len);

ssize_t
net_recv_peek(socket_t sock);

ssize_t
net_recv_all(socket_t sock, void *buf, size_t len);

ssize_t
net_send(socket_t sock, const void *buf, size_t len);

ssize_t
net_send_all(socket_t sock, const void *buf, size_t len);

bool
net_close(socket_t sock);
#endif
