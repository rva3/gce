#include "common.h"

int __modsi3(int a, int b) {
  u32 ua = (u32)a, ub = (u32)b, r;
  if (a < 0)
    ua = 0 - ua;
  if (b < 0)
    ub = 0 - ub;
  gce_div32(ua, ub, &r);
  return (int)(a < 0 ? 0 - r : r);
}
