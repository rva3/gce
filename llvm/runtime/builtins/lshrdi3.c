#include "common.h"

u64 __lshrdi3(u64 a, int n) {
  wide x = {a};
  if (!n)
    return a;
  if (n >= 32) {
    x.w.lo = x.w.hi >> (n - 32);
    x.w.hi = 0;
  } else {
    x.w.lo = (x.w.lo >> n) | (x.w.hi << (32 - n));
    x.w.hi >>= n;
  }
  return x.u;
}
