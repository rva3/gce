#pragma once
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#define O_CREAT 0x40
#define O_TRUNC 0x200
#define O_APPEND 0x400
#define O_NONBLOCK 0x800
#define F_GETFL 3
#define F_SETFL 4
int open(const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);
