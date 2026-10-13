#include "common.h"

void *memmove(void *dst, const void *src, unsigned n) {
  unsigned char *d = dst;
  const unsigned char *s = src;
  if ((u32)d < (u32)s) {
    while (n--)
      *d++ = *s++;
  } else {
    d += n;
    s += n;
    while (n--)
      *--d = *--s;
  }
  return dst;
}
