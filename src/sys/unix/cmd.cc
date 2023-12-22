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

#ifdef DEBUG
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

        // Clean up all inherited fds.
        // https://stackoverflow.com/a/918469
        const int fromfd = STDERR_FILENO + 1;

        // cmake -
        // CHECK_FUNCTION_EXISTS(closefrom HAVE_CLOSEFROM)
        #ifdef HAVE_CLOSEFROM
        closefrom(fromfd);
        #else
        int maxfd = sysconf(_SC_OPEN_MAX);
        if (maxfd < fromfd) {
            maxfd = 65536;
        }
        for (int i = fromfd; i < maxfd-1; i++) { close(i); }
        #endif

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
