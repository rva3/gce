#define UART ((volatile unsigned int *)0x11002000)
void putc(char c) { UART[0] = c; }
void puts(const char *s) {
  while (*s)
    putc(*s++);
}
void _start(void) {
  puts("hi\n");
  for(;;);
}
