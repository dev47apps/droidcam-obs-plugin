#include "net.h"
#include <unistd.h>

bool
net_init(void) {
    return true;
}

void
net_cleanup(void) {
}

bool
net_close(socket_t socket) {
    return !close(socket);
}
