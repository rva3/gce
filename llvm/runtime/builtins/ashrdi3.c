#include "common.h"

i64 __ashrdi3(i64 a, int n) {
  wide x;
  x.s = a;
  if (!n)
    return a;
  if (n >= 32) {
    x.w.lo = (int)x.w.hi >> (n - 32);
    x.w.hi = (int)x.w.hi >> 31;
  } else {
    x.w.lo = (x.w.lo >> n) | (x.w.hi << (32 - n));
    x.w.hi = (int)x.w.hi >> n;
  }
  return x.s;
}
