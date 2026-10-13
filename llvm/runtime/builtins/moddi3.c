#include "common.h"

i64 __moddi3(i64 a, i64 b) {
  wide x, y, r;
  x.s = a;
  y.s = b;
  u32 sx = x.w.hi >> 31;
  if (sx)
    gce_neg(&x);
  if (y.w.hi >> 31)
    gce_neg(&y);
  wide q;
  gce_div64(&x, &y, &q, &r);
  if (sx)
    gce_neg(&r);
  return r.s;
}
