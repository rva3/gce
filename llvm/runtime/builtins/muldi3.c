#include "common.h"

u64 __muldi3(u64 a, u64 b) {
  wide x = {a}, y = {b}, r;
  u32 x0 = x.w.lo & 65535, x1 = x.w.lo >> 16, y0 = y.w.lo & 65535,
      y1 = y.w.lo >> 16;
  /* Prevent LLVM's wide-multiply idiom recognition from reconstructing an
   * i64 multiplication here, which would call __muldi3 recursively. */
  volatile u32 low_product = x0 * y0;
  u32 p = low_product, t = x1 * y0 + (p >> 16), w = t & 65535, k = t >> 16;
  w += x0 * y1;
  r.w.lo = x.w.lo * y.w.lo;
  r.w.hi = x1 * y1 + k + (w >> 16) + x.w.lo * y.w.hi + x.w.hi * y.w.lo;
  return r.u;
}
