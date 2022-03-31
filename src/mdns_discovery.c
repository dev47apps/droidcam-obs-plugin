/*
Copyright (C) 2021 DEV47APPS, github.com/dev47apps

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
#include <stdio.h>

#ifdef _WIN32
# define snprintf sprintf_s
#else
# include <arpa/inet.h>
# include <sys/errno.h>
# include <sys/socket.h>
# include <netdb.h>

# pragma GCC diagnostic ignored "-Wunused-function"
#endif

#include "mdns.h"
#include "net.h"
#include "device_discovery.h"
#include "plugin.h"

#define MDNS_STRING_LIMIT(s, l) while (0) { if (s.length >= l) s.length = l - 1; }

MDNS::MDNS() {
    sock = INVALID_SOCKET;
    query_id = -1;
}

MDNS::~MDNS() {
}

// Callback handling parsing answers to queries sent
static int
query_callback(int sock, const struct sockaddr* from, size_t addrlen, mdns_entry_type_t entry_type,
               uint16_t query_id, uint16_t rtype, uint16_t rclass, uint32_t ttl, const void* data,
               size_t size, size_t name_offset, size_t name_length, size_t record_offset,
               size_t record_length, void* user_data)
{
    (void)sizeof(sock);
    (void)sizeof(query_id);
    (void)sizeof(name_length);
    (void)sizeof(record_length);
    (void)sizeof(ttl);

    MDNS *mdnsMgr = (MDNS *)user_data;
    char addrbuffer[INET6_ADDRSTRLEN] = {0};

    Device dev;
    mdns_string_t fromaddrstr;

    if (from->sa_family == AF_INET) {
        fromaddrstr.str = inet_ntop(AF_INET, &((const struct sockaddr_in*) from)->sin_addr, addrbuffer, (socklen_t)addrlen);
    } else {
        fromaddrstr.str = inet_ntop(AF_INET6, &((const struct sockaddr_in6*) from)->sin6_addr, addrbuffer, (socklen_t)addrlen);
        elog("IPv6 is not supported");
        return 0;
    }

    if (fromaddrstr.str) {
        fromaddrstr.length = strnlen(fromaddrstr.str, sizeof(addrbuffer));
    } else {
        elog("mDNS: error parsing fromaddress: %s", strerror(errno));
        return 0;
    }

#ifdef DEBUG
    const char *entry_name, *record_name;
    switch (rtype) {
        case MDNS_RECORDTYPE_PTR:
            record_name = "PTR";
            break;
        case MDNS_RECORDTYPE_SRV:
            record_name = "SRV";
            break;
        case MDNS_RECORDTYPE_A:
            record_name = "A";
            break;
        case MDNS_RECORDTYPE_AAAA:
            record_name = "AAAA";
            break;
        default:
            record_name = "ANY";
    }

    switch (entry_type) {
        case MDNS_ENTRYTYPE_QUESTION:
            entry_name = "QUESTION";
            break;
        case MDNS_ENTRYTYPE_ANSWER:
            entry_name = "ANSWER";
            break;
        case MDNS_ENTRYTYPE_AUTHORITY:
            entry_name = "AUTHORITY";
            break;
        default:
            entry_name = "ADDITIONAL";
    }
#endif

    dlog("mDNS: reply from %.*s entry=%s record=%s", MDNS_STRING_FORMAT(fromaddrstr), entry_name, record_name);

    if (entry_type == MDNS_ENTRYTYPE_ANSWER) {
        mdns_string_t record = mdns_record_parse_ptr(data, size, record_offset, record_length, dev.serial, sizeof(dev.serial)-1);
        dlog("mDNS: ANSWER name=%.*s", MDNS_STRING_FORMAT(record));

        if (fromaddrstr.length >= (sizeof(dev.address)-1)) {
            elog("error: fromaddress size too large: %d", (int) fromaddrstr.length);
            return 0;
        }

        for (ssize_t i = 0; i < DEVICES_LIMIT; i++) {
            if (mdnsMgr->deviceList[i] == NULL) {
                dlog("adding new device with serial '%.*s'", MDNS_STRING_FORMAT(record));
                mdnsMgr->deviceList[i] = new Device();
                memset(mdnsMgr->deviceList[i]->model, 0, sizeof(dev.model));
                memset(mdnsMgr->deviceList[i]->serial, 0, sizeof(dev.serial));
                memset(mdnsMgr->deviceList[i]->address, 0, sizeof(dev.address));

                memcpy(mdnsMgr->deviceList[i]->serial, MDNS_STRING_ARGS(record));
                memcpy(mdnsMgr->deviceList[i]->address, MDNS_STRING_ARGS(fromaddrstr));
                break;
            }
            if (memcmp(mdnsMgr->deviceList[i]->serial, MDNS_STRING_ARGS(record)) == 0) {
                dlog("updating address for '%.*s'", MDNS_STRING_FORMAT(record));
                memset(mdnsMgr->deviceList[i]->address, 0, sizeof(dev.address));
                memcpy(mdnsMgr->deviceList[i]->address, MDNS_STRING_ARGS(fromaddrstr));
                break;
            }
        }
        return 0;
    }


    if (entry_type != MDNS_ENTRYTYPE_ADDITIONAL) {
        return 0;
    }

    // ADDITIONAL section
    if (rtype == MDNS_RECORDTYPE_SRV) {
        char entrybuffer[256];
        mdns_record_srv_t srv = mdns_record_parse_srv(data, size, record_offset, record_length, entrybuffer, sizeof(entrybuffer));
        dlog("mDNS: SRV %.*s port=%d", MDNS_STRING_FORMAT(srv.name), srv.port);
        // not using the port from here...
        return 0;
    }

    if (rtype == MDNS_RECORDTYPE_TXT) {
        mdns_record_txt_t txtbuf[512];
        mdns_string_t entry = mdns_string_extract(data, size, &name_offset, dev.serial, sizeof(dev.serial)-1);
        ssize_t parsed = mdns_record_parse_txt(data, size, record_offset, record_length, txtbuf, ARRAY_LEN(txtbuf));

        for (ssize_t t = 0; t < parsed; t++) {
            if (txtbuf[t].value.length == 0) {
                dlog("mDNS: TXT %.*s", MDNS_STRING_FORMAT(txtbuf[t].key));
                continue;
            }

            dlog("mDNS: TXT %.*s = %.*s", MDNS_STRING_FORMAT(txtbuf[t].key), MDNS_STRING_FORMAT(txtbuf[t].value));

            // DroidCam TXT records
            const char* key_model   = "model";
            const char* key_address = "address";

            for (ssize_t i = 0; i < DEVICES_LIMIT; i++) {
                if (mdnsMgr->deviceList[i] == NULL)
                    break;

                if (memcmp(mdnsMgr->deviceList[i]->serial, MDNS_STRING_ARGS(entry)) == 0) {
                    if (strncmp(key_model, MDNS_STRING_ARGS(txtbuf[t].key)) == 0)
                    {
                        MDNS_STRING_LIMIT(txtbuf[t].value, sizeof(dev.model));
                        dlog("using model='%.*s' for '%.*s'", MDNS_STRING_FORMAT(txtbuf[t].value), MDNS_STRING_FORMAT(entry));
                        memset(mdnsMgr->deviceList[i]->model, 0, sizeof(dev.model));
                        snprintf(mdnsMgr->deviceList[i]->model, sizeof(dev.model), "%.*s (WiFi)", MDNS_STRING_FORMAT(txtbuf[t].value));
                    }
                    else if (strncmp(key_address, MDNS_STRING_ARGS(txtbuf[t].key)) == 0)
                    {
                        MDNS_STRING_LIMIT(txtbuf[t].value, sizeof(dev.address));
                        dlog("updating wifi address for '%.*s'", MDNS_STRING_FORMAT(entry));
                        memset(mdnsMgr->deviceList[i]->address, 0, sizeof(dev.address));
                        memcpy(mdnsMgr->deviceList[i]->address, MDNS_STRING_ARGS(txtbuf[t].value));
                    }
                    break;
                }
            }
        }
    }

    return 0;
}

void MDNS::FinishQuery(void) {
    dlog("mDNS: reading replies for socket %d", sock);
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    do {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);

        if (select(sock+1, &set, 0, 0, &timeout) <= 0)
            break;

       if (FD_ISSET(sock, &set)) {
            void* user_data = this;
            mdns_query_recv(sock, buffer, sizeof(buffer), query_callback, user_data, query_id);
       }

       FD_SET(sock, &set);
    } while (1);
}

Device* MDNS::NextDevice(void) {
    if (sock != INVALID_SOCKET) {
        ClearDeviceList();
        FinishQuery();
        mdns_socket_close(sock);
        sock = INVALID_SOCKET;
    }

    if (iter < DEVICES_LIMIT && deviceList[iter]) {
        Device* dev = deviceList[iter++];
        return dev;
    }

    return 0;
}

bool MDNS::Query(const char* service_name) {
    const char* record_name = "ANY";
    const mdns_record_type_t record = MDNS_RECORDTYPE_ANY;

    if (sock >= 0) {
        mdns_socket_close(sock);
    }

    sock = mdns_socket_open_ipv4(0);
    if (sock < 0) {
        elog("socket(): %s", strerror(errno));
        return 0;
    }

    dlog("mDNS: got socket %d", sock);
    dlog("mDNS: query %s %s\n", service_name, record_name);
    query_id = mdns_query_send(sock, record, service_name, strlen(service_name), buffer, sizeof(buffer), 0);
    if (query_id < 0) {
        elog("Failed to send mDNS query: %s\n", strerror(errno));
        mdns_socket_close(sock);
        sock = INVALID_SOCKET;
        return 0;
    }

    return 1;
}
