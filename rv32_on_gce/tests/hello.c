#define UART ((volatile unsigned int *)0x11002000)
void _start(void) {
  UART[0] = 'h';
  UART[0] = 'o';
  for(;;);
}
