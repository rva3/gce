/* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
 * compiler-rt's generic widening conversion preserves signaling NaNs.
 * Quiet them at the GCE conversion boundary, as APFloat does. Keep the
 * vendored source unchanged and inline its integer-only conversion here.
 */
#define __extendsfdf2 __gce_extendsfdf2_raw
#include "softfloat/extendsfdf2.c"
#undef __extendsfdf2

double __extendsfdf2(float a) {
  union { float f; uint32_t u; } bits = {a};
  if ((bits.u & UINT32_C(0x7fffffff)) > UINT32_C(0x7f800000))
    bits.u |= UINT32_C(0x00400000);
  return __gce_extendsfdf2_raw(bits.f);
}
