#include "common.h"

u32 __udivsi3(u32 a, u32 b) {
  u32 r;
  return gce_div32(a, b, &r);
}
