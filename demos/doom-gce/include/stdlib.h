#pragma once
#include <stddef.h>

#ifndef NULL
#define NULL 0
#endif

void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

void abort(void);
void exit(int status);

int atoi(const char *nptr);
long atol(const char *nptr);
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
double atof(const char *nptr);
double strtod(const char *nptr, char **endptr);

int rand(void);
void srand(unsigned int seed);
#define RAND_MAX 0x7FFFFFFF

char *getenv(const char *name);
int system(const char *string);
int atexit(void (*func)(void));
int abs(int x);
long labs(long x);
