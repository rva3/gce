#define UART ((volatile unsigned int *)0x11002000)
#define CSRR(v, csr) __asm__ volatile("csrr %0, " #csr : "=r"(v))
#define CSRW(csr, v) __asm__ volatile("csrw " #csr ", %0" ::"r"(v))
#define CSRRW(o, csr, v)                                                       \
  __asm__ volatile("csrrw %0, " #csr ", %1" : "=r"(o) : "r"(v))
#define CSRRS(o, csr, v)                                                       \
  __asm__ volatile("csrrs %0, " #csr ", %1" : "=r"(o) : "r"(v))
#define CSRRC(o, csr, v)                                                       \
  __asm__ volatile("csrrc %0, " #csr ", %1" : "=r"(o) : "r"(v))
#define CSRRWI(o, csr, v)                                                      \
  __asm__ volatile("csrrwi %0, " #csr ", %1" : "=r"(o) : "i"(v))
#define CSRRSI(o, csr, v)                                                      \
  __asm__ volatile("csrrsi %0, " #csr ", %1" : "=r"(o) : "i"(v))
#define CSRRCI(o, csr, v)                                                      \
  __asm__ volatile("csrrci %0, " #csr ", %1" : "=r"(o) : "i"(v))

void trap_handler(void) {
  unsigned cause, epc, st;
  extern char ecalls[];
  CSRR(cause, mcause);
  CSRR(epc, mepc);
  CSRR(st, mstatus);
  UART[0] = '0' + (cause == 11);
  UART[0] = '0' + (epc == (unsigned)ecalls);
  UART[0] = '0' + (((st >> 11) & 3) == 3);
  CSRW(mepc, epc + 4);
  __asm__ volatile("mret");
}

static unsigned sbi_set_timer(unsigned t) {
  unsigned ret;
  __asm__ volatile("mv a0, %1\n"
                   "li a1, 0\n"
                   "li a6, 0\n"
                   "li a7, 0\n"
                   "ecall\n"
                   "mv %0, a0"
                   : "=r"(ret)
                   : "r"(t)
                   : "memory", "a0", "a1", "a6", "a7", "t0", "t1", "t2");
  return ret;
}

volatile int ticked = 0;

void timer_handler(void) {
  __asm__ volatile("addi sp, sp, -32\n"
                   "sw ra, 28(sp)\n"
                   "sw a0, 24(sp)\n"
                   "sw a1, 20(sp)\n"
                   "sw a6, 16(sp)\n"
                   "sw a7, 12(sp)\n"
                   "sw t0, 8(sp)\n"
                   "sw t1, 4(sp)\n"
                   "sw t2, 0(sp)\n");
  {
    unsigned cause;
    CSRR(cause, mcause);
    if (cause == 0x80000007u) {
      UART[0] = '1';
      ticked = 1;
    } else {
      UART[0] = '0';
    }
    sbi_set_timer(0xFFFFFFFFu);
  }
  __asm__ volatile("lw t2, 0(sp)\n"
                   "lw t1, 4(sp)\n"
                   "lw t0, 8(sp)\n"
                   "lw a7, 12(sp)\n"
                   "lw a6, 16(sp)\n"
                   "lw a1, 20(sp)\n"
                   "lw a0, 24(sp)\n"
                   "lw ra, 28(sp)\n"
                   "addi sp, sp, 32\n"
                   "mret");
}

void _start(void) {
  unsigned v, o;
  CSRR(v, mhartid);
  UART[0] = '0' + (v == 0);
  CSRR(v, misa);
  UART[0] = '0' + (v == 0x40001101u);
  CSRRW(o, mscratch, 0x1234);
  UART[0] = '0' + (o == 0);
  CSRR(v, mscratch);
  UART[0] = '0' + (v == 0x1234);
  CSRRS(o, mie, 0x80);
  UART[0] = '0' + (o == 0);
  CSRRC(o, mie, 0x80);
  UART[0] = '0' + (o == 0x80);
  CSRRWI(o, mscratch, 0x1F);
  CSRR(v, mscratch);
  UART[0] = '0' + (o == 0x1234 && v == 0x1F);
  CSRRSI(o, mscratch, 0x10);
  CSRR(v, mscratch);
  UART[0] = '0' + (o == 0x1F && v == 0x1F);
  CSRRCI(o, mscratch, 0x1F);
  CSRR(v, mscratch);
  UART[0] = '0' + (o == 0x1F && v == 0x0);
  {
    unsigned t1, t2, c;
    __asm__ volatile("csrr %0, time" : "=r"(t1));
    __asm__ volatile("csrr %0, time" : "=r"(t2));
    __asm__ volatile("csrr %0, cycle" : "=r"(c));
    UART[0] = '0' + (t2 > t1 && c > 0);
  }
  CSRW(mtvec, trap_handler);
  __asm__ volatile("li a7, 0xff\necalls: ecall" ::
                       : "memory", "a0", "a1", "a2", "a3", "a4", "a5",
                         "a7", "t0", "t1", "t2", "t5", "t6");
  UART[0] = '1';
  {
    unsigned t, to = 10000000;
    CSRW(mtvec, timer_handler);
    __asm__ volatile("csrs mie, %0" :: "r"(0x80));
    __asm__ volatile("csrs mstatus, %0" :: "r"(0x8));
    __asm__ volatile("csrr %0, time" : "=r"(t));
    UART[0] = '0' + (sbi_set_timer(t + 20000) == 0);
    while (!ticked && --to)
      ;
    UART[0] = '0' + (ticked && to);
  }
  {
    volatile unsigned *cmp = (volatile unsigned *)0x02004000;
    volatile unsigned *mtm = (volatile unsigned *)0x0200bff8;
    unsigned t1, t2, c;
    __asm__ volatile("csrc mstatus, %0" :: "r"(0x8));
    __asm__ volatile("csrc mie, %0" :: "r"(0x80));
    *cmp = 0x12345678;
    c = *cmp;
    t1 = *mtm;
    t2 = *mtm;
    UART[0] = '0' + (c == 0x12345678 && t2 > t1);
    *cmp = 0xFFFFFFFFu;
    UART[0] = '0' + (*cmp == 0xFFFFFFFFu);
  }
  for (;;);
}
