#include <string.h>

#include <plugin.h>
#include <net.h>
#include <device_discovery.h>
#include <command.h>

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
    dlog("~test_adb");
}

#define REQ "GET /robots.txt HTTP/1.1\r\nHost: 1.1.1.1\r\n\r\n"
void test_net(void) {
    char buffer[1024];
    const char* request = REQ;
    int len;
    socket_t socket;
    ilog("test_net()");

    net_init();
    socket = net_connect("1.1.1.1", 80);
    if (socket == INVALID_SOCKET) {
        elog("connect failed");
        goto out;
    }

    dlog("sending request");
    if ((len = net_send_all(socket, request, strlen(request))) <= 0) {
        elog("send failed");
        goto out;
    }

    dlog("getting reply");
    if ((len = net_recv(socket, buffer, sizeof(buffer))) <= 0) {
        elog("recv failed");
        goto out;
    }

    ilog("got %d bytes", len);
    dlog("%.*s", len, buffer);

out:
    if (socket != INVALID_SOCKET) net_close(socket);
    net_cleanup();
    dlog("~test_net");
}

int main(int argc, char** argv) {
    (void) argc; (void) argv;
    test_exec();
    test_adb();
    test_net();
    return 0;
}
