#define UART ((volatile unsigned int *)0x11002000)
#define CSRR(v, csr) __asm__ volatile("csrr %0, " #csr : "=r"(v))
#define CSRW(csr, v) __asm__ volatile("csrw " #csr ", %0" ::"r"(v))

volatile unsigned buf[4] = {0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00};
volatile unsigned want_cause, want_tval;

void trap_handler(void) {
  __asm__ volatile("addi sp, sp, -64\n"
                   "sw ra, 60(sp)\n"
                   "sw a0, 56(sp)\n"
                   "sw a1, 52(sp)\n"
                   "sw a2, 48(sp)\n"
                   "sw a3, 44(sp)\n"
                   "sw a4, 40(sp)\n"
                   "sw a5, 36(sp)\n"
                   "sw a6, 32(sp)\n"
                   "sw a7, 28(sp)\n"
                   "sw t0, 24(sp)\n"
                   "sw t1, 20(sp)\n"
                   "sw t2, 16(sp)\n"
                   "sw t3, 12(sp)\n"
                   "sw t4, 8(sp)\n"
                   "sw t5, 4(sp)\n"
                   "sw t6, 0(sp)\n");
  {
    unsigned cause, epc, tval;
    CSRR(cause, mcause);
    CSRR(epc, mepc);
    CSRR(tval, mtval);
    UART[0] = '0' + (cause == want_cause);
    UART[0] = '0' + (tval == want_tval);
    CSRW(mepc, epc + 4);
  }
  __asm__ volatile("lw t6, 0(sp)\n"
                   "lw t5, 4(sp)\n"
                   "lw t4, 8(sp)\n"
                   "lw t3, 12(sp)\n"
                   "lw t2, 16(sp)\n"
                   "lw t1, 20(sp)\n"
                   "lw t0, 24(sp)\n"
                   "lw a7, 28(sp)\n"
                   "lw a6, 32(sp)\n"
                   "lw a5, 36(sp)\n"
                   "lw a4, 40(sp)\n"
                   "lw a3, 44(sp)\n"
                   "lw a2, 48(sp)\n"
                   "lw a1, 52(sp)\n"
                   "lw a0, 56(sp)\n"
                   "lw ra, 60(sp)\n"
                   "addi sp, sp, 64\n"
                   "mret");
}

void _start(void) {
  unsigned b = (unsigned)buf;
  unsigned v = 0x5a5a5a5a;
  CSRW(mtvec, trap_handler);
  want_cause = 4;
  want_tval = b + 1;
  __asm__ volatile("lw x0, 0(%0)" ::"r"(b + 1) : "memory");
  want_tval = b + 2;
  __asm__ volatile("lw x0, 0(%0)" ::"r"(b + 2) : "memory");
  want_tval = b + 1;
  __asm__ volatile("lh x0, 0(%0)" ::"r"(b + 1) : "memory");
  want_cause = 6;
  want_tval = b + 3;
  __asm__ volatile("sw %0, 0(%1)" ::"r"(v), "r"(b + 3) : "memory");
  UART[0] = '1';
  want_tval = b + 1;
  __asm__ volatile("sh %0, 0(%1)" ::"r"(v), "r"(b + 1) : "memory");
  UART[0] = '1';
  for (;;);
}
