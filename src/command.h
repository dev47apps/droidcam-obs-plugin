#ifndef COMMAND_H
#define COMMAND_H

#include <stdbool.h>
#include <inttypes.h>

#ifdef _WIN32

 // not needed here, but winsock2.h must never be included AFTER windows.h
# include <winsock2.h>
# include <windows.h>

# define strtok_r strtok_s
# define snprintf sprintf_s
# define PATH_SEPARATOR '\\'
# define PRIexitcode "lu"
// <https://stackoverflow.com/a/44383330/1987178>
# ifdef _WIN64
#   define PRIsizet PRIu64
# else
#   define PRIsizet PRIu32
# endif
# define PROCESS_NONE NULL
  typedef HANDLE process_t;
  typedef DWORD exit_code_t;

#else

# include <sys/types.h>
# define PATH_SEPARATOR '/'
# define PRIsizet "zu"
# define PRIexitcode "d"
# define PROCESS_NONE -1
  typedef pid_t process_t;
  typedef int exit_code_t;

#endif

# define NO_EXIT_CODE -1

enum process_result {
    PROCESS_SUCCESS,
    PROCESS_ERROR_GENERIC,
    PROCESS_ERROR_MISSING_BINARY,
};

enum process_result cmd_execute(const char *path, const char *const argv[], process_t *handle, char* output, size_t out_size);

bool cmd_simple_wait(process_t pid, exit_code_t *exit_code);
bool argv_to_string(const char *const *argv, char *buf, size_t bufsize);
bool process_check_success(process_t proc, const char *name);
void process_print_error(enum process_result err, const char *const argv[]);

#endif
