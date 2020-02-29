#include <errno.h>
#include <string.h>
#include "plugin.h"
#include "plugin_properties.h"
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
net_connect_and_ping(const char* ip, uint16_t port) {
    int len;
    char buf[8];
    const char *ping_req = PING_REQ;

    socket_t sock = net_connect(ip, port);
    if (sock == INVALID_SOCKET) {
        return INVALID_SOCKET;
    }
    if ((len = net_send_all(sock, ping_req, sizeof(PING_REQ)-1)) <= 0) {
        elog("send(ping) failed");
        net_close(sock);
        return INVALID_SOCKET;
    }

    if ((len = net_recv(sock, buf, sizeof(buf))) <= 0) {
        elog("recv(ping) failed");
        net_close(sock);
        return INVALID_SOCKET;
    }

    if (len != 4 || memcmp(buf, "pong", 4) != 0) {
        elog("recv invalid data: %.*s", len, buf);
        net_close(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

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
net_recv(socket_t sock, void *buf, size_t len) {
    return recv(sock, buf, len, 0);
}

ssize_t
net_recv_all(socket_t sock, void *buf, size_t len) {
    return recv(sock, buf, len, MSG_WAITALL);
}

ssize_t
net_send(socket_t sock, const void *buf, size_t len) {
    return send(sock, buf, len, 0);
}

ssize_t
net_send_all(socket_t sock, const void *buf, size_t len) {
    ssize_t w = 0;
    while (len > 0) {
        w = send(sock, buf, len, 0);
        if (w == -1) {
            return -1;
        }
        len -= w;
        buf = (char *) buf + w;
    }
    return w;
}

bool
net_close(socket_t sock)
{
    shutdown(sock, SHUT_RDWR);
#ifdef __WINDOWS__
    return !closesocket(sock);
#else
    return !close(sock);
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
