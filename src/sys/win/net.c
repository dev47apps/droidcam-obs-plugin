#include "net.h"

#include "plugin.h"

// FIXME needed? OBS would have initd this probably
bool
net_init(void) {
    WSADATA wsa;
    int res = WSAStartup(MAKEWORD(2, 2), &wsa) < 0;
    if (res < 0) {
        elog("WSAStartup failed with error %d", res);
        return false;
    }
    return true;
}

void
net_cleanup(void) {
    WSACleanup();
}

bool
net_close(socket_t socket) {
    return !closesocket(socket);
}
