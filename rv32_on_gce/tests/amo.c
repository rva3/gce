#define UART ((volatile unsigned int *)0x11002000)
void _start(void) {
  volatile int mem = 0x10;
  volatile int *p = &mem;
  volatile unsigned *up = (volatile unsigned *)&mem;
  UART[0] = '0' + (__atomic_exchange_n(p, 0x41, __ATOMIC_RELAXED) == 0x10);
  UART[0] = '0' + (__atomic_fetch_add(p, 5, __ATOMIC_RELAXED) == 0x41);
  UART[0] = '0' + (__atomic_fetch_xor(p, 0xFF, __ATOMIC_RELAXED) == 0x46);
  UART[0] =
      '0' + (__atomic_fetch_and(p, 0xF0, __ATOMIC_RELAXED) == (0x46 ^ 0xFF));
  UART[0] = '0' + (__atomic_fetch_or(p, 0x0F, __ATOMIC_RELAXED) == 0xB0);
  UART[0] = '0' + (__atomic_fetch_min(p, 0x100, __ATOMIC_RELAXED) == 0xBF);
  UART[0] = '0' + (__atomic_fetch_max(p, 0x500, __ATOMIC_RELAXED) == 0xBF);
  UART[0] = '0' + (__atomic_fetch_min(up, 0x50u, __ATOMIC_RELAXED) == 0x500);
  UART[0] = '0' + (__atomic_fetch_max(up, 0x40u, __ATOMIC_RELAXED) == 0x50);
  UART[0] = '0' + (__atomic_exchange_n(p, 0x42, __ATOMIC_SEQ_CST) == 0x50);
  int expect = 0x42, want = 0x43;
  int ok = __atomic_compare_exchange_n(p, &expect, want, 0, __ATOMIC_RELAXED,
                                       __ATOMIC_RELAXED);
  UART[0] = '0' + (ok && expect == 0x42);
  int r;
  __asm__ volatile("sc.w %0, %1, 0(%2)" : "=r"(r) : "r"(0x99), "r"(p));
  UART[0] = '0' + (r == 1 && *p == 0x43);
  for (;;)
    ;
}
