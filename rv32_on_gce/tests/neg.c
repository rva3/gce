#define UART ((volatile unsigned int *)0x11002000)
void _start(void) {
  volatile int a = -8;
  volatile int b = -5;
  volatile int c;
  c = a >> 2;
  UART[0] = c + 2;
  UART[0] = '0' + (c + 2);
  c = (a < b);
  UART[0] = '0' + c;
  c = ((unsigned)a < (unsigned)b);
  UART[0] = '0' + c;
  volatile unsigned char *p = (unsigned char *)0x40001000;
  *p = 0xFF;
  c = *(volatile signed char *)0x40001000;
  UART[0] = '0' + (c + 1);
  c = *(volatile unsigned char *)0x40001000;
  UART[0] = c;
  volatile unsigned short *q = (unsigned short *)0x40001000;
  *q = 0xFFFF;
  c = *(volatile signed short *)0x40001000;
  UART[0] = '0' + (c + 1);
  c = *(volatile unsigned short *)0x40001000;
  UART[0] = c & 0xFF;
  if (a < b)
    UART[0] = 'T';
  else
    UART[0] = 'F';
  if (a >= b)
    UART[0] = 'T';
  else
    UART[0] = 'F';
  if ((unsigned)a < (unsigned)b)
    UART[0] = 'T';
  else
    UART[0] = 'F';
  volatile int sh = 2;
  c = a >> sh;
  UART[0] = '0' + (c + 2);
  for(;;);
}
