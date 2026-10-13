#define UART ((volatile unsigned int *)0x11002000)
#define CLINT_CMP ((volatile unsigned int *)0x02004000)
#define CLINT_CMPH ((volatile unsigned int *)0x02004004)
#define CLINT_MTIME ((volatile unsigned int *)0x0200bff8)
#define CSRR(v, csr) __asm__ volatile("csrr %0, " #csr : "=r"(v))
#define CSRW(csr, v) __asm__ volatile("csrw " #csr ", %0" :: "r"(v))

volatile int ticks = 0;

static void prog_cmp(unsigned long delta) {
  unsigned now = *CLINT_MTIME;
  *CLINT_CMP = now + delta;
  *CLINT_CMPH = 0;
}

void tick_handler(void) {
  __asm__ volatile("addi sp, sp, -112\n"
                   "sw ra, 108(sp)\n"
                   "sw a0, 104(sp)\n"
                   "sw a1, 100(sp)\n"
                   "sw a2, 96(sp)\n"
                   "sw a3, 92(sp)\n"
                   "sw a4, 88(sp)\n"
                   "sw a5, 84(sp)\n"
                   "sw a6, 80(sp)\n"
                   "sw a7, 76(sp)\n"
                   "sw t0, 72(sp)\n"
                   "sw t1, 68(sp)\n"
                   "sw t2, 64(sp)\n"
                   "sw t3, 60(sp)\n"
                   "sw t4, 56(sp)\n"
                   "sw t5, 52(sp)\n"
                   "sw t6, 48(sp)\n"
                   "sw s0, 44(sp)\n"
                   "sw s1, 40(sp)\n"
                   "sw s2, 36(sp)\n"
                   "sw s3, 32(sp)\n"
                   "sw s4, 28(sp)\n"
                   "sw s5, 24(sp)\n"
                   "sw s6, 20(sp)\n"
                   "sw s7, 16(sp)\n"
                   "sw s8, 12(sp)\n"
                   "sw s9, 8(sp)\n"
                   "sw s10, 4(sp)\n"
                   "sw s11, 0(sp)\n");
  {
    unsigned cause;
    CSRR(cause, mcause);
    if (cause == 0x80000007u) {
      ticks++;
      prog_cmp(20000);
    }
  }
  __asm__ volatile("lw s11, 0(sp)\n"
                   "lw s10, 4(sp)\n"
                   "lw s9, 8(sp)\n"
                   "lw s8, 12(sp)\n"
                   "lw s7, 16(sp)\n"
                   "lw s6, 20(sp)\n"
                   "lw s5, 24(sp)\n"
                   "lw s4, 28(sp)\n"
                   "lw s3, 32(sp)\n"
                   "lw s2, 36(sp)\n"
                   "lw s1, 40(sp)\n"
                   "lw s0, 44(sp)\n"
                   "lw t6, 48(sp)\n"
                   "lw t5, 52(sp)\n"
                   "lw t4, 56(sp)\n"
                   "lw t3, 60(sp)\n"
                   "lw t2, 64(sp)\n"
                   "lw t1, 68(sp)\n"
                   "lw t0, 72(sp)\n"
                   "lw a7, 76(sp)\n"
                   "lw a6, 80(sp)\n"
                   "lw a5, 84(sp)\n"
                   "lw a4, 88(sp)\n"
                   "lw a3, 92(sp)\n"
                   "lw a2, 96(sp)\n"
                   "lw a1, 100(sp)\n"
                   "lw a0, 104(sp)\n"
                   "lw ra, 108(sp)\n"
                   "addi sp, sp, 112\n"
                   "mret");
}

void _start(void) {
  int i;
  CSRW(mtvec, tick_handler);
  __asm__ volatile("csrs mie, %0" :: "r"(0x80));
  __asm__ volatile("csrs mstatus, %0" :: "r"(0x8));
  for (i = 0; i < 5; i++) {
    unsigned to = 10000000;
    int seen = ticks;
    prog_cmp(20000);
    while (ticks == seen && --to)
      ;
    UART[0] = '0' + (ticks != seen && to);
  }
  for (;;)
    ;
}
