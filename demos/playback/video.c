#include <stdint.h>
#include <stddef.h>
#include "../shims.h"
#include "video_data.h"

static inline void uart_write_buf(const uint8_t *buf, uint32_t len) {
    const uint8_t *end = buf + len;
    while (buf < end) {
        while (!(UART[5] & 0x20))
            ;
        UART[0] = (unsigned int)*buf++;
    }
}

int main(void) {
    static const uint8_t init_seq[] = "\033[?25l\033[2J\033[H";
    uart_write_buf(init_seq, sizeof(init_seq) - 1);
    while (1) {
        for (uint32_t i = 0; i < NUM_FRAMES; i++) {
            uint32_t frame_start = *APXGPT4;
            const uint8_t *data = video_payload + frame_offsets[i];
            uint32_t len = frame_lens[i];
            uart_write_buf(data, len);

            while ((*APXGPT4 - frame_start) < FRAME_TICKS)
                ;
        }
    }
    for (;;)
        ;
}

#ifdef __riscv
void _start(void) {
    main();
}
#endif
