#include "common.h"

u64 __ashldi3(u64 a, int n) {
  wide x = {a};
  if (!n)
    return a;
  if (n >= 32) {
    x.w.hi = x.w.lo << (n - 32);
    x.w.lo = 0;
  } else {
    x.w.hi = (x.w.hi << n) | (x.w.lo >> (32 - n));
    x.w.lo <<= n;
  }
  return x.u;
}
