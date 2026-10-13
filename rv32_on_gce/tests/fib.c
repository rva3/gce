#define UART ((volatile unsigned int *)0x11002000)
__attribute__((noinline)) int fib(int n) {
  if (n < 2)
    return n;
  return fib(n - 1) + fib(n - 2);
}
void _start(void) {
  UART[0] = '0' + fib(10) - 55; // fib10=55 -> '0'
  for(;;);
}
