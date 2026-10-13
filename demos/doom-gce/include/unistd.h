#pragma once
#include <stddef.h>
#include <sys/types.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int close(int fd);
int isatty(int fd);
int fileno(void *stream);
int usleep(unsigned int usec);
unsigned int sleep(unsigned int seconds);
