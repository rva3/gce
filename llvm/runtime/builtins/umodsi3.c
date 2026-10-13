#include "common.h"

u32 __umodsi3(u32 a, u32 b) {
  u32 r;
  gce_div32(a, b, &r);
  return r;
}
