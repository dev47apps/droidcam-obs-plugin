// modern glibc will complain without this
#define _DEFAULT_SOURCE

#include "command.h"

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

enum process_result
cmd_execute(const char *path, const char *const argv[], pid_t *pid, char* out, size_t out_size) {
    int fd[2];
    char scratch[256];
    enum process_result ret = PROCESS_SUCCESS;

#ifdef TEST
    argv_to_string(argv, scratch, sizeof(scratch));
    dlog("exec %s", scratch);
#endif

    if (pipe(fd) == -1) {
        elog("pipe: %s", strerror(errno));
        return PROCESS_ERROR_GENERIC;
    }

    *pid = fork();
    if (*pid == -1) {
        elog("fork: %s", strerror(errno));
        ret = PROCESS_ERROR_GENERIC;
        goto end;
    }

    if (*pid > 0) {
        // parent close write side
        close(fd[1]); fd[1] = -1;

        size_t n;
        if (out != NULL && out_size > 2) {
            n = read(fd[0], out, out_size - 1);
            if (n < 0 || n >= out_size) {
                elog("parent read: %s", strerror(errno));
                ret = PROCESS_ERROR_GENERIC;
                goto end;
            }
            if (n > 0) {
                out[n] = 0;
            }
            // n == 0
        }
        do {
            n = read(fd[0], scratch, sizeof(scratch));
        } while (n > 0);
    }
    else if (*pid == 0) {
        if (dup2(fd[1], STDOUT_FILENO) < 0) {
            elog("dup2 stdout: %s", strerror(errno));
            _exit(PROCESS_ERROR_GENERIC);
        }
        if (dup2(fd[1], STDERR_FILENO) < 0) {
            elog("dup2 stderr: %s", strerror(errno));
            _exit(PROCESS_ERROR_GENERIC);
        }
        close(fd[0]);
        close(fd[1]);
        execvp(path, (char *const *)argv);
        if (errno == ENOENT)
            ret = PROCESS_ERROR_MISSING_BINARY;
        else
            ret = PROCESS_ERROR_GENERIC;
        elog("exec: %s", strerror(errno));
        _exit(ret);
    }

end:
    if (fd[0] != -1) {
        close(fd[0]);
    }
    if (fd[1] != -1) {
        close(fd[1]);
    }
    return ret;
}


bool
cmd_simple_wait(pid_t pid, int *exit_code) {
    int status;
    int code;
    if (waitpid(pid, &status, 0) == -1 || !WIFEXITED(status)) {
        // could not wait, or exited unexpectedly, probably by a signal
        code = -1;
        elog("waitpid: %s", strerror(errno));
    } else {
        code = WEXITSTATUS(status);
    }
    if (exit_code) {
        *exit_code = code;
    }
    return !code;
}
