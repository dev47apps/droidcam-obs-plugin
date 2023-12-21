// Copyright (C) 2022 DEV47APPS, github.com/dev47apps
#include <stdio.h>

#include <util/threading.h>

#include "net.h"
#include "command.h"
#include "plugin.h"
#include "plugin_properties.h"
#include "device_discovery.h"

void test_exec(void) {
    enum process_result pr;
    process_t process;
    char out[32];
    ilog("test_exec()");

    const char *cmd1[] = {"echo", "message", NULL};
    pr = cmd_execute(cmd1[0], cmd1, &process, out, sizeof(out));
    if (pr != PROCESS_SUCCESS) {
        process_print_error(pr, cmd1);
    }
    else {
        process_check_success(process, "cmd1");
        ilog("OK > %s", out);
    }

    char looong[1024] = {0};
    memset(looong, 'z', sizeof(looong)-1);
    const char *cmd2[] = {"echo", looong, NULL};
    pr = cmd_execute(cmd2[0], cmd2, &process, NULL, 0);
    if (pr != PROCESS_SUCCESS) {
        process_print_error(pr, cmd2);
    }
    else {
        process_check_success(process, "cmd2");
        ilog("OK > (no output read)");
    }

    out[0] = 0;
    looong[16] = '\0';
    const char *cmd3[] = {"echo", looong, "args", NULL};
    pr = cmd_execute(cmd3[0], cmd3, &process, out, sizeof(out));
    if (pr != PROCESS_SUCCESS) {
        process_print_error(pr, cmd3);
    }
    else {
        process_check_success(process, "cmd3");
        ilog("OK > %s", out);
    }

    dlog("~test_exec");
}

void test_adb(void) {
    ilog("test_adb()");
    int count = 0;
    Device* dev;
    AdbMgr adbMgr;
    adbMgr.Reload();
    adbMgr.ResetIter();
    while ((dev = adbMgr.NextDevice()) != NULL) {
        adbMgr.GetModel(dev);
        ilog("dev: serial=%s state=%s model=%s", dev->serial, dev->state, dev->model);
        count++;
    }
    if (count == 0) {
        elog("Failed: No devices found");
    }
    else {
        dlog("test_adb: found %d devices", count);
        const char* serial = "empty1";
        dev = adbMgr.GetDevice(serial, strlen(serial));

        ilog("device '%s' returned %p @ %d", serial, dev, adbMgr.Iter());
        if (!dev)           elog("Failed: Expected device '%s' was not loaded\n", serial);
        if (!adbMgr.Iter()) elog("Failed: Expected device '%s' to update Iter\n", serial);
    }

    dlog("~test_adb");
}

#define REQ "GET / HTTP/1.1\r\nHost: %s\r\n\r\n"
void test_net(const char *host, int port) {
    char buffer[1024];
    int len;
    socket_t socket;
    ilog("test_net()");

    socket = net_connect(host, port);
    if (socket == INVALID_SOCKET) {
        elog("connect failed");
        goto out;
    }

    snprintf(buffer, sizeof(buffer), REQ, host);
    dlog("sending request");
    if ((len = net_send_all(socket, buffer, strlen(buffer))) <= 0) {
        elog("send failed");
        goto out;
    }

    buffer[0] = 0;
    dlog("getting reply");
    if ((len = net_recv(socket, buffer, sizeof(buffer))) <= 0) {
        elog("recv failed");
        goto out;
    }

    ilog("test_net: Got %d Bytes", len);
    dlog("%.*s", len, buffer);

out:
    if (socket != INVALID_SOCKET) net_close(socket);
    dlog("~test_net");
}

static void *proxy_run(void *data) {
    int proxy_port = *(int *) data;
    dlog("test_proxy() thread");
    test_net(localhost_ip, proxy_port);
    test_net(localhost_ip, proxy_port);
    test_net(localhost_ip, proxy_port);
    return 0;
}

void test_proxy(int proxy_port) {
    pthread_t thr0,thr1,thr2;
    pthread_create(&thr0, NULL, proxy_run, &proxy_port);
    pthread_create(&thr1, NULL, proxy_run, &proxy_port);
    pthread_create(&thr2, NULL, proxy_run, &proxy_port);
    pthread_join(thr0, NULL);
    pthread_join(thr1, NULL);
    pthread_join(thr2, NULL);

    Sleep(1000);

    pthread_create(&thr0, NULL, proxy_run, &proxy_port);
    pthread_create(&thr1, NULL, proxy_run, &proxy_port);
    pthread_create(&thr2, NULL, proxy_run, &proxy_port);
    pthread_join(thr0, NULL);
    pthread_join(thr1, NULL);
    pthread_join(thr2, NULL);
}

void test_ios(void) {
    ilog("test_ios()");
    int count = 0;
    int usb_port = 0;
    Device* dev;
    USBMux iosMgr;
    iosMgr.Reload();
    iosMgr.ResetIter();
    while ((dev = iosMgr.NextDevice()) != NULL) {
        iosMgr.GetModel(dev);
        ilog("dev: serial=%s handle=%d model=%s", dev->serial, dev->handle, dev->model);
        count++;
    }

    if (count) {
        iosMgr.ResetIter();
        dev = iosMgr.NextDevice();
        int sock = iosMgr.Connect(dev, 4747, &usb_port);
        if (sock > 0 && usb_port > 0) {
            test_net(localhost_ip, usb_port);
            test_proxy(usb_port);
            net_close(sock);
        }
        else {
            elog("Failed: Connect failed");
        }
    }
    else {
        elog("Failed: No devices found");
    }
    dlog("~test_ios");
}

int main(int argc, char** argv) {
    (void) argc;
    (void) argv;

    net_init();
    test_exec();
    test_adb();
    #ifdef __APPLE__
    test_ios();
    #endif
    test_net("1.1.1.1", 80);
    net_cleanup();
    return 0;
}
