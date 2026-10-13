#define UART ((volatile unsigned int*)0x11002000)
void _start(void){
 UART[0]='a';
 __asm__ volatile("fence");
 UART[0]='b';
 __asm__ volatile("ecall");
 UART[0]='F'; // unreachable
 for(;;);
}
