#pragma once
#define LOG_INFO 2
#define LOG_ERROR 0
#define LOG_WARNING 1

#include <stdio.h>

#define blog(log_level, fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
