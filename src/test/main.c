#include <string.h>

#include <plugin.h>
#include <command.h>
#include <adb_command.h>

void test_exec(void) {
    enum process_result pr;
    process_t process;
    char out[2] = {0};

    const char *cmd1[] = {"echo", "message", NULL};
    pr = cmd_execute(cmd1[0], cmd1, &process, out, sizeof(out));
    if (pr != PROCESS_SUCCESS) process_print_error(pr, cmd1);
    else process_check_success(process, "cmd1");
    dlog(":%s", out);

    char looong[1024] = {0};
    memset(looong, 'z', sizeof(looong)-1);
    const char *cmd2[] = {"echo", looong, NULL};
    pr = cmd_execute(cmd2[0], cmd2, &process, NULL, 0);
    if (pr != PROCESS_SUCCESS) process_print_error(pr, cmd2);
    else process_check_success(process, "cmd2");
    dlog(":%s", out);

    looong[251] = '\0';
    const char *cmd3[] = {"echo", looong, "args", NULL};
    pr = cmd_execute(cmd3[0], cmd3, &process, NULL, 0);
    if (pr != PROCESS_SUCCESS) process_print_error(pr, cmd3);
    else process_check_success(process, "cmd3");
    dlog(":%s", out);
}

void test_adb(void) {
    AdbMgr adbMgr;
    adbMgr.reload();
}

int main(int argc, char** argv) {
    (void) argc; (void) argv;
    test_exec();
    test_adb();
    return 0;
}
