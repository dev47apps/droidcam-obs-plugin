#include <errno.h>
#include <string.h>
#include "plugin.h"
#include "net.h"

#ifdef __WINDOWS__
  typedef int socklen_t;
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <arpa/inet.h>
# include <unistd.h>
# define SOCKET_ERROR -1
  typedef struct sockaddr_in SOCKADDR_IN;
  typedef struct sockaddr SOCKADDR;
  typedef struct in_addr IN_ADDR;
#endif

socket_t
net_connect(const char* ip, uint16_t port) {
    dlog("connect %s:%d", ip, port);
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        elog("socket(): %s", strerror(errno));
        return INVALID_SOCKET;
    }

    SOCKADDR_IN sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = inet_addr(ip);
    sin.sin_port = htons(port);

    if (connect(sock, (SOCKADDR *) &sin, sizeof(sin)) == SOCKET_ERROR) {
        elog("connect(): %s", strerror(errno));
        net_close(sock);
        return INVALID_SOCKET;
    }

    return sock;
}

ssize_t
net_recv(socket_t socket, void *buf, size_t len) {
    return recv(socket, buf, len, 0);
}

ssize_t
net_recv_all(socket_t socket, void *buf, size_t len) {
    return recv(socket, buf, len, MSG_WAITALL);
}

ssize_t
net_send(socket_t socket, const void *buf, size_t len) {
    return send(socket, buf, len, 0);
}

ssize_t
net_send_all(socket_t socket, const void *buf, size_t len) {
    ssize_t w = 0;
    while (len > 0) {
        w = send(socket, buf, len, 0);
        if (w == -1) {
            return -1;
        }
        len -= w;
        buf = (char *) buf + w;
    }
    return w;
}

bool
net_close(socket_t socket)
{
    shutdown(socket, SHUT_RDWR);
#ifdef __WINDOWS__
    return !closesocket(socket);
#else
    return !close(socket);
}
#endif

bool
net_init(void) {
#ifdef __WINDOWS__
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
    if (res < 0) {
        elog("WSAStartup failed with error %d", res);
        return false;
    }
#endif
    return true;
}

void
net_cleanup(void) {
#ifdef __WINDOWS__
    WSACleanup();
#endif
}
