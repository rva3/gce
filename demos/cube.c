#include <stdint.h>
#include "shims.h"

#define WIDTH  80
#define HEIGHT 24

static int mul(int a, int b) {
    int res = 0;
    unsigned int u_a = (a < 0) ? -a : a;
    unsigned int u_b = (b < 0) ? -b : b;
    while (u_b) {
        if (u_b & 1) res += u_a;
        u_a <<= 1;
        u_b >>= 1;
    }
    return ((a < 0) ^ (b < 0)) ? -res : res;
}

static const int8_t sin_lut[64] = {
      0,  6, 12, 18, 24, 30, 35, 40, 45, 50, 54, 57, 60, 62, 63, 63,
     64, 63, 63, 62, 60, 57, 54, 50, 45, 40, 35, 30, 24, 18, 12,  6,
      0, -6,-12,-18,-24,-30,-35,-40,-45,-50,-54,-57,-60,-62,-63,-63,
    -64,-63,-63,-62,-60,-57,-54,-50,-45,-40,-35,-30,-24,-18,-12, -6
};

static int get_sin(int a) { return sin_lut[a & 63]; }
static int get_cos(int a) { return sin_lut[(a + 16) & 63]; }

static char fb[HEIGHT][WIDTH] __attribute__((aligned(4)));

static void clear_fb(void) {
    uint32_t *p = (uint32_t *)fb;
    uint32_t space_x4 = 0x20202020u; // ' ' ' ' ' ' ' '
    int count = (WIDTH * HEIGHT) >> 2;
    
    for (int i = 0; i < count; i++) {
        p[i] = space_x4;
    }
}

static void draw_line(int x0, int y0, int x1, int y1, char c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (1) {
        if (x0 >= 0 && x0 < WIDTH && y0 >= 0 && y0 < HEIGHT)
            fb[y0][x0] = c;
        if (x0 == x1 && y0 == y1) break;
        int e2 = err << 1;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static const int cube_verts[8][3] = {
    {-16, -16, -16}, { 16, -16, -16}, { 16,  16, -16}, {-16,  16, -16},
    {-16, -16,  16}, { 16, -16,  16}, { 16,  16,  16}, {-16,  16,  16}
};

static const int cube_edges[12][2] = {
    {0,1}, {1,2}, {2,3}, {3,0},
    {4,5}, {5,6}, {6,7}, {7,4},
    {0,4}, {1,5}, {2,6}, {3,7}
};

void render_cube(int angle) {
    clear_fb();

    int sin_a = get_sin(angle), cos_a = get_cos(angle);
    int sin_b = get_sin(angle >> 1), cos_b = get_cos(angle >> 1);

    int proj_x[8], proj_y[8];

    for (int i = 0; i < 8; i++) {
        int x = cube_verts[i][0];
        int y = cube_verts[i][1];
        int z = cube_verts[i][2];

        /* Rotate Y-axis */
        int x1 = (mul(x, cos_a) - mul(z, sin_a)) >> 6;
        int z1 = (mul(x, sin_a) + mul(z, cos_a)) >> 6;

        /* Rotate X-axis */
        int y2 = (mul(y, cos_b) - mul(z1, sin_b)) >> 6;

        /*
         * Centered at (40, 12):
         * X proj: 40 + x1          -> Max range: [17, 63]
         * Y proj: 12 + (y2 * 3 / 8) -> Max range: [2, 22]
         */
        proj_x[i] = 40 + x1;
        proj_y[i] = 12 + ((mul(y2, 3)) >> 3);
    }

    /* Draw edges */
    for (int i = 0; i < 12; i++) {
        int v0 = cube_edges[i][0];
        int v1 = cube_edges[i][1];
        draw_line(proj_x[v0], proj_y[v0], proj_x[v1], proj_y[v1], '#');
    }

    puts("\033[H");
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            putc(fb[y][x]);
        }
        putc('\n');
    }
}

int main(void) {
    puts("\033[?25l\033[2J");

    int angle = 0;
    for(;;)
        render_cube(angle++);
}

#ifdef __riscv
void _start(void) {
    main();
}
#endif
