/*
Copyright (C) 2022 DEV47APPS, github.com/dev47apps

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef _WIN32
# include <sys/select.h>
#endif
#include <util/platform.h>

#include <vector>

#include "plugin.h"
#include "plugin_properties.h"
#include "net.h"
#include "device_discovery.h"

void *proxy_run(void *data);

Proxy::Proxy(DeviceDiscovery* device_discovery) {
    port_local = 0;
    port_remote = 0;
    thread_active = 0;
    proxy_device = NULL;
    proxy_sock = INVALID_SOCKET;
    discovery_mgr = device_discovery;
}

Proxy::~Proxy() {
    if (thread_active) {
        thread_active = 0;
        pthread_join(pthr, NULL);
        net_close(proxy_sock);
    }
}

int Proxy::Start(Device *dev, int remote_port) {
    proxy_device = dev;
    port_remote = remote_port;

    if (thread_active == 0) {
        if (proxy_sock != INVALID_SOCKET)
            net_close(proxy_sock);

        proxy_sock = net_listen(localhost_ip, 0);
        port_local = (proxy_sock != INVALID_SOCKET)
                    ? net_listen_port(proxy_sock) : 0;

        thread_active = port_local > 0
            && pthread_create(&pthr, NULL, proxy_run, this) == 0;

        if(!thread_active) {
            elog("Error creating iproxy server/thread");
            return 0;
        }
    }

    return port_local;
}

struct proxy_conn {
    socket_t client;
    socket_t remote;
    proxy_conn(socket_t c, socket_t r) {
        client = c; remote = r;
    }
};

#define BUF_SIZE 32768
#ifdef TEST
#define vlog dlog
#else
#define vlog(...)
#endif

void* proxy_run(void *data) {
    fd_set set;
    socket_t maxfd;
    std::vector<struct proxy_conn*> list;
    Proxy *proxy = (Proxy*) data;

    auto buffer = (uint8_t*) bmalloc(BUF_SIZE);
    FD_ZERO(&set);
    maxfd = 0;

    while (proxy->thread_active) {
        socket_t client = net_accept(proxy->proxy_sock);

        if (client != INVALID_SOCKET) {
            // todo: make connect function generic, usbmux hacked in here for now
            #ifdef _WIN32
            auto usbmux = (USBMux*) proxy->discovery_mgr;
            int rc = usbmux->usbmuxd_connect(
                (uint32_t) proxy->proxy_device->handle,
                (short) proxy->port_remote);

            #elif __linux__
            int rc = usbmuxd_connect(
                (uint32_t) proxy->proxy_device->handle,
                (short) proxy->port_remote);

            #elif __APPLE__
            int rc = net_connect(
                (const char*) proxy->proxy_device->address,
                proxy->port_remote);

            #else
            #error Unknown System
            #endif

            if (rc > 0) {
                socket_t remote = rc;
                vlog("proxy: %llu <==> %llu created", client, remote);
                set_nonblock(remote, 1);
                set_recv_timeout(remote, 1);
                list.push_back(new proxy_conn(client, remote));

                FD_SET(client, &set); if (client > maxfd) maxfd = client;
                FD_SET(remote, &set); if (remote > maxfd) maxfd = remote;
            }
            else {
                elog("proxy: remote connection failed");
                net_close(client);
            }
        }

        if (list.size() == 0) {
            os_sleep_ms(5);
            continue;
        }

        fd_set read_fds = set;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 5000;
        int rc = select(maxfd+1, &read_fds, NULL, NULL, &timeout);
        if (rc == 0)
            continue;

        if (rc < 0) {
            WSAErrno();
            elog("proxy select failed (%d): %s", errno, strerror(errno));
            os_sleep_ms(5);
            continue;
        }

        vlog("select: %d read_fds", rc);

        auto size = list.size();
        auto i = std::begin(list);
        while (i != std::end(list)) {
            int err = 0;
            auto elem = *i;

            if (FD_ISSET(elem->client, &read_fds)) {
                ssize_t r =         net_recv (elem->client, buffer, BUF_SIZE);
                ssize_t s = r > 0 ? net_send_all(elem->remote, buffer, r) : 0;
                if (r <= 0 || s <= 0) err = 1;
                vlog("proxy: %llu  ==> %llu // r=%ld s=%ld err=%d",
                    elem->client, elem->remote, r, s, err);
            }

            if (FD_ISSET(elem->remote, &read_fds)) {
                ssize_t r =         net_recv (elem->remote, buffer, BUF_SIZE);
                ssize_t s = r > 0 ? net_send_all(elem->client, buffer, r) : 0;
                if (r <= 0 || s <= 0) err = 1;
                vlog("proxy: %llu <==  %llu // r=%ld s=%ld err=%d",
                    elem->client, elem->remote, r, s, err);
            }

            if (err) {
                vlog("proxy: %llu <==> %llu close", elem->client, elem->remote);
                i = list.erase(i);
                net_close(elem->client);
                net_close(elem->remote);
                FD_CLR(elem->client, &set);
                FD_CLR(elem->remote, &set);
                delete elem;
            }
            else i++;
        }

        if (size != list.size()) {
            for (auto elem : list) {
                if (elem->client > maxfd) maxfd = elem->client;
                if (elem->remote > maxfd) maxfd = elem->remote;
            }
        }
    } // while(active)

    bfree(buffer);
    while (list.size()) {
        auto elem = (struct proxy_conn *) list.back();
        vlog("proxy: %llu <==> %llu close", elem->client, elem->remote);
        net_close(elem->client);
        net_close(elem->remote);
        list.pop_back();
        delete elem;
    }

    return 0;
}
