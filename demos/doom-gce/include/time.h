#pragma once
#include <stddef.h>
#ifndef NULL
#define NULL 0
#endif
typedef long time_t;
typedef long clock_t;
#define CLOCKS_PER_SEC 1000000L
time_t time(time_t *t);
clock_t clock(void);
