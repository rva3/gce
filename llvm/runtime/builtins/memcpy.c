#include "common.h"

void *memcpy(void *dst, const void *src, unsigned n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  while (n--)
    *d++ = *s++;
  return dst;
}
