#define UART ((volatile unsigned int *)0x11002000)
__attribute__((noinline)) void putc(char c) { UART[0] = c; }
__attribute__((noinline)) void puts(const char *s) {
  while (*s)
    putc(*s++);
}
void _start(void) {
  volatile char buf[4];
  buf[0] = 'h';
  buf[1] = 'i';
  buf[2] = '\n';
  buf[3] = 0;
  puts(buf);
  for(;;);
}
