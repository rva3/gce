#pragma once
struct stat {
    long st_size;
};
#define S_IFDIR 0040000
#define S_IFREG 0100000
int stat(const char *path, struct stat *buf);
int mkdir(const char *path, unsigned int mode);
