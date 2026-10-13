#include "common.h"

u64 __udivdi3(u64 a, u64 b) {
  wide x = {a}, y = {b}, q, r;
  gce_div64(&x, &y, &q, &r);
  return q.u;
}
