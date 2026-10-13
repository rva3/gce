#include "common.h"

int memcmp(const void *a, const void *b, unsigned n) {
  const unsigned char *x = a, *y = b;
  while (n--) {
    int d = *x++ - *y++;
    if (d)
      return d;
  }
  return 0;
}
