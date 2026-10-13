#define WIDTH  50
#define HEIGHT 30
#define MAX_SNAKE 50

#include <stddef.h>
#include <stdint.h>
#include "shims.h"

static inline int uart_getc_nonblocking(void) {
    if (UART[5] & 0x1)
        return UART[0];
    else
        return -1;
}

extern void delay_ms(unsigned int ms) { usleep(ms * 1000); }

static void vt_puts(const char *s) {
    while (*s) {
        putc(*s++);
    }
}

static void vt_clear(void) {
    vt_puts("\033[2J\033[H"); // Clear screen & home cursor
}

static void vt_putnum(int n) {
    if (n >= 10) {
        putc('0' + (n / 10));
    }
    putc('0' + (n % 10));
}

static void vt_gotorc(int r, int c) {
    putc('\033');
    putc('[');
    vt_putnum(r);
    putc(';');
    vt_putnum(c);
    putc('H');
}

/* Game State */
typedef struct { int x, y; } Point;
static Point snake[MAX_SNAKE];
static int len;
static int dx, dy;
static Point food;

void game_init(void) {
    len = 3;
    dx = 1;
    dy = 0;
    food.x = 12;
    food.y = 5;

    for (int i = 0; i < len; i++) {
        snake[i].x = 5 - i;
        snake[i].y = 5;
    }

    vt_clear();
    for (int r = 1; r <= HEIGHT; r++) {
        vt_gotorc(r, 1);       putc('#');
        vt_gotorc(r, WIDTH);   putc('#');
    }
    for (int c = 1; c <= WIDTH; c++) {
        vt_gotorc(1, c);       putc('#');
        vt_gotorc(HEIGHT, c);  putc('#');
    }
}

void game_loop(void) {
    game_init();

    while (1) {
        int input = uart_getc_nonblocking();
        if (input == 'w' && dy == 0)      { dx = 0;  dy = -1; }
        else if (input == 's' && dy == 0) { dx = 0;  dy = 1;  }
        else if (input == 'a' && dx == 0) { dx = -1; dy = 0;  }
        else if (input == 'd' && dx == 0) { dx = 1;  dy = 0;  }

        Point tail = snake[len - 1];
        vt_gotorc(tail.y, tail.x);
        putc(' ');

        for (int i = len - 1; i > 0; i--) {
            snake[i] = snake[i - 1];
        }
        snake[0].x += dx;
        snake[0].y += dy;

        if (snake[0].x <= 1 || snake[0].x >= WIDTH ||
            snake[0].y <= 1 || snake[0].y >= HEIGHT) {
            vt_gotorc(HEIGHT + 2, 1);
            vt_puts("GAME OVER\r\n");
            break;
        }

        if (snake[0].x == food.x && snake[0].y == food.y) {
            if (len < MAX_SNAKE) len++;
            food.x = (food.x * 3 + 1) % (WIDTH - 2) + 2;
            food.y = (food.y * 3 + 2) % (HEIGHT - 2) + 2;
        }

        vt_gotorc(food.y, food.x);   putc('*');
        vt_gotorc(snake[0].y, snake[0].x); putc('O');

        delay_ms(150);
    }
}

int main(void) {
    game_loop();
    for(;;);
}

#ifdef __riscv
void _start(void) {
    main();
}
#endif
