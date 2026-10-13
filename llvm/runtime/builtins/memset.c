#include "common.h"

void *memset(void *dst, int value, unsigned n) {
  unsigned char *d = dst;
  while (n--)
    *d++ = (unsigned char)value;
  return dst;
}
