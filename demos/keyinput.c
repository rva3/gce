#include "shims.h"

int main(void) {
  puts("key test start\n");

  for (;;) {
    if (UART[5] & 0x01) {
      int key = UART[0];
      puts("input: ");
      putc(key);
      puts("\n");
    }
  }
}

#ifdef __riscv
void _start(void) { main(); }
#endif
