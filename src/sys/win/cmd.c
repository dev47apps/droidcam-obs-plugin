/*
Copyright (C) 2020 github.com/aramg

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

#include "plugin.h"
#include "command.h"

enum process_result
cmd_execute(const char *path, const char *const argv[], HANDLE *handle, char* out, size_t out_size) {
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    SECURITY_ATTRIBUTES saAttr;
    *handle = NULL;

    // Set the bInheritHandle flag so pipe handles are inherited.
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    // Create a pipe for the child process's STDOUT.
    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        elog("StdoutRd CreatePipe error");
        return PROCESS_ERROR_GENERIC;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        elog("Stdout SetHandleInformation error");
        return PROCESS_ERROR_GENERIC;
    }

    // Create the child process.
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    int flags = CREATE_NO_WINDOW;
    memset(&si, 0, sizeof(STARTUPINFOW));
    memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    si.cb = sizeof(STARTUPINFO);
    si.hStdError  = hChildStd_OUT_Wr;
    si.hStdOutput = hChildStd_OUT_Wr;
    si.dwFlags |= STARTF_USESTDHANDLES;


    char cmd[256];
    if (argv_to_string(argv, cmd, sizeof(cmd))) {
        return PROCESS_ERROR_GENERIC;
    }

    dlog("exec %s", cmd);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, TRUE, flags, NULL, NULL, &si, &pi)) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            return PROCESS_ERROR_MISSING_BINARY;
        }
        return PROCESS_ERROR_GENERIC;
    }

    *handle = pi.hProcess;
    CloseHandle(hChildStd_OUT_Wr);

    // Read output
    DWORD n;
    BOOL bSuccess = FALSE;
    int scratch[256];

    if (out != NULL && out_size > 2) {
        bSuccess = ReadFile(hChildStd_OUT_Rd, out, (DWORD)out_size, &n, NULL);
        if (!bSuccess || n >= out_size) {
            elog("parent read: %d", GetLastError());
            return PROCESS_ERROR_GENERIC;
        }
        if (n > 0) {
            out[n] = 0;
        }
        // n == 0
    }
    do {
        bSuccess = ReadFile(hChildStd_OUT_Rd, scratch, sizeof(scratch), &n, NULL);
    } while (bSuccess && n > 0);

    return PROCESS_SUCCESS;
}

bool
cmd_simple_wait(HANDLE handle, DWORD *exit_code) {
    DWORD code;
    if (WaitForSingleObject(handle, INFINITE) != WAIT_OBJECT_0
            || !GetExitCodeProcess(handle, &code)) {
        // could not wait or retrieve the exit code
        code = -1; // max value, it's unsigned
    }
    if (exit_code) {
        *exit_code = code;
    }
    return !code;
}

