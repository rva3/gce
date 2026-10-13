#define UART ((volatile unsigned int*)0x11002000)
void _start(void){
 volatile unsigned a=0x12345678, b=0xdeadbeef;
 volatile int sa=(int)0x80000000, sb=-1;
 volatile unsigned c;
 c = a * b; UART[0]=c;                                            // MUL
 UART[0]=(unsigned)(((long long)(int)a * (int)b >> 32) & 0xff);   // MULH
 UART[0]=(unsigned)(((long long)(int)a * b >> 32) & 0xff);        // MULHSU
 UART[0]=(unsigned)(((unsigned long long)a * b >> 32) & 0xff);    // MULHU
 UART[0]=(unsigned)(((long long)sa * sb >> 32) & 0xff);           // MIN*-1 -> 0
 {
  volatile int d = 7, e = 3, f = -7, g = -3, z = 0;
  UART[0] = d / e;   // 2
  UART[0] = f / e;   // -2 -> fe
  UART[0] = d / g;   // -2
  UART[0] = f / g;   // 2
  UART[0] = d / z;   // -1 -> ff
  UART[0] = sa / sb; // MIN -> 00 low
  UART[0] = d % e;   // 1
  UART[0] = f % e;   // -1 -> ff
  UART[0] = sa % sb; // 0
  UART[0] = d % z;   // 7
 }
 {
  volatile unsigned d = 7, e = 3, z = 0, big = 0xffffffffu;
  UART[0] = d / e;   // 2
  UART[0] = big / e; // 0x55555555 -> 55
  UART[0] = d / z;   // ffffffff -> ff
  UART[0] = d % e;   // 1
  UART[0] = big % e; // 0
  UART[0] = d % z;   // 7
 }
 while(1){}
}
