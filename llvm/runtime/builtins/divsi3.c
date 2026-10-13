#include "common.h"

int __divsi3(int a, int b) {
  u32 ua = (u32)a, ub = (u32)b, r;
  if (a < 0)
    ua = 0 - ua;
  if (b < 0)
    ub = 0 - ub;
  u32 q = gce_div32(ua, ub, &r);
  return (int)((a < 0) != (b < 0) ? 0 - q : q);
}
