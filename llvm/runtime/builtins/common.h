/* Shared implementation for the freestanding integer runtime helpers. */
#ifndef GCE_BUILTINS_COMMON_H
#define GCE_BUILTINS_COMMON_H

typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;

typedef union {
  u64 u;
  i64 s;
  struct {
    u32 lo, hi;
  } w;
} wide;

extern void __gce_trap(void) __attribute__((noreturn));

static inline u32 gce_div32(u32 a, u32 b, u32 *rp) {
  u32 q = 0, r = 0;
  if (!b)
    __gce_trap();
  for (unsigned n = 32; n; --n) {
    u32 carry = r >> 31;
    r = (r << 1) | ((a >> (n - 1)) & 1);
    if (carry || r >= b) {
      r -= b;
      q |= 1u << (n - 1);
    }
  }
  *rp = r;
  return q;
}

static inline void gce_neg(wide *a) {
  a->w.lo = 0 - a->w.lo;
  a->w.hi = ~a->w.hi + (a->w.lo == 0);
}

static inline void gce_div64(const wide *ap, const wide *bp, wide *qp,
                             wide *rp) {
  wide a = *ap, b = *bp, q = {0}, r = {0};
  if (!(b.w.lo | b.w.hi))
    __gce_trap();
  for (unsigned n = 64; n; --n) {
    u32 carry = r.w.hi >> 31;
    r.w.hi = (r.w.hi << 1) | (r.w.lo >> 31);
    r.w.lo = (r.w.lo << 1) | (a.w.hi >> 31);
    a.w.hi = (a.w.hi << 1) | (a.w.lo >> 31);
    a.w.lo <<= 1;
    q.w.hi = (q.w.hi << 1) | (q.w.lo >> 31);
    q.w.lo <<= 1;
    if (carry || r.w.hi > b.w.hi ||
        (r.w.hi == b.w.hi && r.w.lo >= b.w.lo)) {
      u32 borrow = r.w.lo < b.w.lo;
      r.w.lo -= b.w.lo;
      r.w.hi -= b.w.hi + borrow;
      q.w.lo |= 1;
    }
  }
  *rp = r;
  *qp = q;
}

#endif
