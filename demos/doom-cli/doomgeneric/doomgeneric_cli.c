// doomgeneric for command line

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "cli_data.h"
#include "i_video.h"
#include "doomgeneric.h"
#include "doomkeys.h"
#include "m_argv.h"

#if DOOMGENERIC_RESX != 320
    #error "ERROR: Invalid DOOMGENERIC_RESX. It must be 320."
#endif
#if DOOMGENERIC_RESY != 200
    #error "ERROR: Invalid DOOMGENERIC_RESY. It must be 200."
#endif

//#define DEBUG_FIXED_TICKRATE


// TODO trap CTRL+C, send reset and re-enable the cursor




/*
 * Options and other common state
 */

static int columns = 80;

static int dest_width;
static int dest_height;
static uint32_t* dest_buffer;

static bool synchronized_updates;

typedef enum {
    cli_mode_sextant = 1,
    cli_mode_quadrant,
    cli_mode_half,
    cli_mode_space,
} cli_mode_t;

static cli_mode_t cli_mode = cli_mode_sextant;

typedef enum {
    cli_colors_24bit = 1,
    cli_colors_8bit,
    cli_colors_4bit,
    cli_colors_3bit,
    cli_colors_light,
    cli_colors_dark,
} cli_colors_t;

static cli_colors_t cli_colors = cli_colors_24bit;

static bool print_stats = true;

// circular buffer of frame times and frame sizes for statistics calculations
#define stats_capacity 20
static uint32_t stats_times[stats_capacity];
static uint32_t stats_sizes[stats_capacity];
static int stats_next;
static int stats_count;

typedef enum keystate_t {
    keystate_off = 0, // key is up and released.
    keystate_down,    // key received initial press, press event sent
    keystate_wait,    // key may be down, we sent release but still waiting for a repeat
    keystate_repeat,  // additional keypress received after repeat rate; key is repeating
} keystate_t;

// We store the last few times each button was pressed in a circular buffer.
// This is used to estimate key repeat rate and key repeat delay.
typedef struct keyinfo_t {
    #define TIME_CAPACITY 5
    uint32_t time[TIME_CAPACITY];
    uint32_t time_next;
    uint32_t time_count;
    keystate_t state;
    bool detected_repeat;
} keyinfo_t;
keyinfo_t keyinfos[256];

// This is the threshold for detecting key repeats (in milliseconds.) Under 10
// FPS (e.g. with the Onramp machine code VM) we can't reliably get keypress
// timings under 5ms.
static uint32_t key_repeat_threshold = 10;

// We store a circular buffer of key repeat delay and rate measurements.
#define KEY_MEASURE_CAPACITY 16
static uint32_t key_measure_next = 0;
static uint32_t key_measure_count = 0;
static uint32_t key_measure_delays[KEY_MEASURE_CAPACITY];
static uint32_t key_measure_rates[KEY_MEASURE_CAPACITY];

/**
 * We choose the "best" values from the above measurement buffers to use as the
 * delay and rate. The initial values are a guess, the default configuration
 * for a typical OS.
 */
uint32_t key_repeat_delay = 500;
uint32_t key_repeat_rate = 100;

// These maximums hopefully keep the game playable even if the user's repeat
// settings are high or our detection is buggy.
#define KEY_REPEAT_DELAY_MAX 500
#define KEY_REPEAT_RATE_MAX 500

/**
 * A circular buffer containing computed key events yet to be dispatched to
 * Doom.
 */
#define KEYBUFFER_CAPACITY 64
struct {
    bool pressed;
    unsigned char key;
} keybuffer[KEYBUFFER_CAPACITY];
int keybuffer_read;
int keybuffer_write;



/*
 * Characters
 */

/**
 * The upper half block.
 */
#define UPPER_HALF "\xE2\x96\x80"  // U+2580: UPPER HALF BLOCK  ▀

/**
 * Quadrant characters.
 *
 * Each of the lower 4 bits of the index is a subpixel. Subpixels are ordered
 * in row-major order, left-to-right and top-to-bottom. (This order differs from
 * their codepoint order in Unicode.)
 *
 *     0 1
 *     2 3
 *
 * Each string contains the UTF-8 encoding of the quadrant character with lit
 * subpixels matching the set bits of the index.
 *
 * See:
 *
 *     https://en.wikipedia.org/wiki/Block_Elements
 */
const char* quadrants[] = {
    " ",             // U+0020: SPACE
    "\xE2\x96\x98",  // U+2598: QUADRANT UPPER LEFT                                  ▘
    "\xE2\x96\x9D",  // U+259D: QUADRANT UPPER RIGHT                                 ▝
    "\xE2\x96\x80",  // U+2580: UPPER HALF BLOCK                                     ▀
    "\xE2\x96\x96",  // U+2596: QUADRANT LOWER LEFT                                  ▖
    "\xE2\x96\x8C",  // U+258C: LEFT HALF BLOCK                                      ▌
    "\xE2\x96\x9E",  // U+259E: QUADRANT UPPER RIGHT AND LOWER LEFT                  ▞
    "\xE2\x96\x9B",  // U+259B: QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER LEFT   ▛
    "\xE2\x96\x97",  // U+2597: QUADRANT LOWER RIGHT                                 ▗
    "\xE2\x96\x9A",  // U+259A: QUADRANT UPPER LEFT AND LOWER RIGHT                  ▚
    "\xE2\x96\x90",  // U+2590: RIGHT HALF BLOCK                                     ▐
    "\xE2\x96\x9C",  // U+259C: QUADRANT UPPER LEFT AND UPPER RIGHT AND LOWER RIGHT  ▜
    "\xE2\x96\x84",  // U+2584: LOWER HALF BLOCK                                     ▄
    "\xE2\x96\x99",  // U+2599: QUADRANT UPPER LEFT AND LOWER LEFT AND LOWER RIGHT   ▙
    "\xE2\x96\x9F",  // U+259F: QUADRANT UPPER RIGHT AND LOWER LEFT AND LOWER RIGHT  ▟
    "\xE2\x96\x88",  // U+2588: FULL BLOCK                                           █
};


/**
 * Sextant characters.
 *
 * Each of the lower 6 bits of the index is a subpixel. Subpixels are ordered in
 * row-major order, left-to-right and top-to-bottom (the same as the numbering
 * scheme in their Unicode names, except the bits are in base-0):
 *
 *     0 1
 *     2 3
 *     4 5
 *
 * Each string contains the UTF-8 encoding of the sextant character with lit
 * subpixels matching the set bits of the index.
 *
 * See:
 *
 *     https://en.wikipedia.org/wiki/Symbols_for_Legacy_Computing
 *
 * Four of the sextant characters are unified with other graphic characters.
 * See section 5 in:
 *
 *     https://www.unicode.org/L2/L2017/17435r-terminals-prop.pdf
 */
const char* sextants[] = {
    " ",                 // U+0020:  SPACE
    "\xF0\x9F\xAC\x80",  // U+1FB00: BLOCK SEXTANT-1      🬀
    "\xF0\x9F\xAC\x81",  // U+1FB01: BLOCK SEXTANT-2      🬁
    "\xF0\x9F\xAC\x82",  // U+1FB02: BLOCK SEXTANT-12     🬂
    "\xF0\x9F\xAC\x83",  // U+1FB03: BLOCK SEXTANT-3      🬃
    "\xF0\x9F\xAC\x84",  // U+1FB04: BLOCK SEXTANT-13     🬄
    "\xF0\x9F\xAC\x85",  // U+1FB05: BLOCK SEXTANT-23     🬅
    "\xF0\x9F\xAC\x86",  // U+1FB06: BLOCK SEXTANT-123    🬆
    "\xF0\x9F\xAC\x87",  // U+1FB07: BLOCK SEXTANT-4      🬇
    "\xF0\x9F\xAC\x88",  // U+1FB08: BLOCK SEXTANT-14     🬈
    "\xF0\x9F\xAC\x89",  // U+1FB09: BLOCK SEXTANT-24     🬉
    "\xF0\x9F\xAC\x8A",  // U+1FB0A: BLOCK SEXTANT-124    🬊
    "\xF0\x9F\xAC\x8B",  // U+1FB0B: BLOCK SEXTANT-34     🬋
    "\xF0\x9F\xAC\x8C",  // U+1FB0C: BLOCK SEXTANT-134    🬌
    "\xF0\x9F\xAC\x8D",  // U+1FB0D: BLOCK SEXTANT-234    🬍
    "\xF0\x9F\xAC\x8E",  // U+1FB0E: BLOCK SEXTANT-1234   🬎
    "\xF0\x9F\xAC\x8F",  // U+1FB0F: BLOCK SEXTANT-5      🬏
    "\xF0\x9F\xAC\x90",  // U+1FB10: BLOCK SEXTANT-15     🬐
    "\xF0\x9F\xAC\x91",  // U+1FB11: BLOCK SEXTANT-25     🬑
    "\xF0\x9F\xAC\x92",  // U+1FB12: BLOCK SEXTANT-125    🬒
    "\xF0\x9F\xAC\x93",  // U+1FB13: BLOCK SEXTANT-35     🬓
    "\xE2\x96\x8C",      // U+258C:  LEFT HALF BLOCK      ▌
    "\xF0\x9F\xAC\x94",  // U+1FB14: BLOCK SEXTANT-235    🬔
    "\xF0\x9F\xAC\x95",  // U+1FB15: BLOCK SEXTANT-1235   🬕
    "\xF0\x9F\xAC\x96",  // U+1FB16: BLOCK SEXTANT-45     🬖
    "\xF0\x9F\xAC\x97",  // U+1FB17: BLOCK SEXTANT-145    🬗
    "\xF0\x9F\xAC\x98",  // U+1FB18: BLOCK SEXTANT-245    🬘
    "\xF0\x9F\xAC\x99",  // U+1FB19: BLOCK SEXTANT-1245   🬙
    "\xF0\x9F\xAC\x9A",  // U+1FB1A: BLOCK SEXTANT-345    🬚
    "\xF0\x9F\xAC\x9B",  // U+1FB1B: BLOCK SEXTANT-1345   🬛
    "\xF0\x9F\xAC\x9C",  // U+1FB1C: BLOCK SEXTANT-2345   🬜
    "\xF0\x9F\xAC\x9D",  // U+1FB1D: BLOCK SEXTANT-12345  🬝
    "\xF0\x9F\xAC\x9E",  // U+1FB1E: BLOCK SEXTANT-6      🬞
    "\xF0\x9F\xAC\x9F",  // U+1FB1F: BLOCK SEXTANT-16     🬟
    "\xF0\x9F\xAC\xA0",  // U+1FB20: BLOCK SEXTANT-26     🬠
    "\xF0\x9F\xAC\xA1",  // U+1FB21: BLOCK SEXTANT-126    🬡
    "\xF0\x9F\xAC\xA2",  // U+1FB22: BLOCK SEXTANT-36     🬢
    "\xF0\x9F\xAC\xA3",  // U+1FB23: BLOCK SEXTANT-136    🬣
    "\xF0\x9F\xAC\xA4",  // U+1FB24: BLOCK SEXTANT-236    🬤
    "\xF0\x9F\xAC\xA5",  // U+1FB25: BLOCK SEXTANT-1236   🬥
    "\xF0\x9F\xAC\xA6",  // U+1FB26: BLOCK SEXTANT-46     🬦
    "\xF0\x9F\xAC\xA7",  // U+1FB27: BLOCK SEXTANT-146    🬧
    "\xE2\x96\x90",      // U+2590:  RIGHT HALF BLOCK     ▐
    "\xF0\x9F\xAC\xA8",  // U+1FB28: BLOCK SEXTANT-1246   🬨
    "\xF0\x9F\xAC\xA9",  // U+1FB29: BLOCK SEXTANT-346    🬩
    "\xF0\x9F\xAC\xAA",  // U+1FB2A: BLOCK SEXTANT-1346   🬪
    "\xF0\x9F\xAC\xAB",  // U+1FB2B: BLOCK SEXTANT-2346   🬫
    "\xF0\x9F\xAC\xAC",  // U+1FB2C: BLOCK SEXTANT-12346  🬬
    "\xF0\x9F\xAC\xAD",  // U+1FB2D: BLOCK SEXTANT-56     🬭
    "\xF0\x9F\xAC\xAE",  // U+1FB2E: BLOCK SEXTANT-156    🬮
    "\xF0\x9F\xAC\xAF",  // U+1FB2F: BLOCK SEXTANT-256    🬯
    "\xF0\x9F\xAC\xB0",  // U+1FB30: BLOCK SEXTANT-1256   🬰
    "\xF0\x9F\xAC\xB1",  // U+1FB31: BLOCK SEXTANT-356    🬱
    "\xF0\x9F\xAC\xB2",  // U+1FB32: BLOCK SEXTANT-1356   🬲
    "\xF0\x9F\xAC\xB3",  // U+1FB33: BLOCK SEXTANT-2356   🬳
    "\xF0\x9F\xAC\xB4",  // U+1FB34: BLOCK SEXTANT-12356  🬴
    "\xF0\x9F\xAC\xB5",  // U+1FB35: BLOCK SEXTANT-456    🬵
    "\xF0\x9F\xAC\xB6",  // U+1FB36: BLOCK SEXTANT-1456   🬶
    "\xF0\x9F\xAC\xB7",  // U+1FB37: BLOCK SEXTANT-2456   🬷
    "\xF0\x9F\xAC\xB8",  // U+1FB38: BLOCK SEXTANT-12456  🬸
    "\xF0\x9F\xAC\xB9",  // U+1FB39: BLOCK SEXTANT-3456   🬹
    "\xF0\x9F\xAC\xBA",  // U+1FB3A: BLOCK SEXTANT-13456  🬺
    "\xF0\x9F\xAC\xBB",  // U+1FB3B: BLOCK SEXTANT-23456  🬻
    "\xE2\x96\x88",      // U+2588:  FULL BLOCK           █
};

// Onramp is not fast. This is much faster than doing decimal conversions.
static const char* u8_to_str[] = {
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15",
    "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31",
    "32", "33", "34", "35", "36", "37", "38", "39", "40", "41", "42", "43", "44", "45", "46", "47",
    "48", "49", "50", "51", "52", "53", "54", "55", "56", "57", "58", "59", "60", "61", "62", "63",
    "64", "65", "66", "67", "68", "69", "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
    "80", "81", "82", "83", "84", "85", "86", "87", "88", "89", "90", "91", "92", "93", "94", "95",
    "96", "97", "98", "99", "100", "101", "102", "103", "104", "105", "106", "107", "108", "109", "110", "111",
    "112", "113", "114", "115", "116", "117", "118", "119", "120", "121", "122", "123", "124", "125", "126", "127",
    "128", "129", "130", "131", "132", "133", "134", "135", "136", "137", "138", "139", "140", "141", "142", "143",
    "144", "145", "146", "147", "148", "149", "150", "151", "152", "153", "154", "155", "156", "157", "158", "159",
    "160", "161", "162", "163", "164", "165", "166", "167", "168", "169", "170", "171", "172", "173", "174", "175",
    "176", "177", "178", "179", "180", "181", "182", "183", "184", "185", "186", "187", "188", "189", "190", "191",
    "192", "193", "194", "195", "196", "197", "198", "199", "200", "201", "202", "203", "204", "205", "206", "207",
    "208", "209", "210", "211", "212", "213", "214", "215", "216", "217", "218", "219", "220", "221", "222", "223",
    "224", "225", "226", "227", "228", "229", "230", "231", "232", "233", "234", "235", "236", "237", "238", "239",
    "240", "241", "242", "243", "244", "245", "246", "247", "248", "249", "250", "251", "252", "253", "254", "255",
};



/*
 * Noise
 *
 * For the paletted modes, we dither by sampling blue noise. The noise is a set
 * of 64 16x16 blue noise images which we rotate through. The noise textures
 * are at the bottom of the file because they're huge.
 *
 * TODO the way the noise is applied right now is bad. It's applied per
 * character; it needs to be applied per pixel. Should just make an apply noise
 * function that takes rgb pointers, force inline it (or wrap it in a macro
 * that checks whether noise is enabled)
 */

bool noise_enabled = true;
static int noise_current = 0;
static uint32_t noise_last_time = 0;
static int noise_speed = 75; // milliseconds

#define NOISE_SAMPLE(x, y) \
        noise_textures[noise_current][((x) & 15) + (((y) & 15) * 16)]
/*
 * Initializes the noise.
 *
 * All values are scaled such that, when 128 is subtracted from them, they
 * become an offset to add to a color channel to dither it.
 */
static void init_noise(void) {
    noise_last_time = DG_GetTicksMs();

    int scale = 0; // percent
    switch (cli_colors) {
        case cli_colors_dark:
        case cli_colors_light:
            scale = 95;
            break;
        case cli_colors_3bit: scale = 30; break;
        case cli_colors_4bit: scale = 20; break;
        case cli_colors_8bit: scale = 2; break;
        default: break;
    }
    if (scale == 0) {
        noise_enabled = false;
        return;
    }
    int base = 255 * (100 - scale) / 200;

    for (size_t tex = 0; tex < noise_texture_count; ++ tex) {
        for (size_t i = 0; i < 16*16; ++i) {
            uint32_t val = noise_textures[tex][i];
            uint32_t blue = (val >> 16) & 0xff;
            uint32_t green = (val >> 8) & 0xff;
            uint32_t red = val & 0xff;
            blue = blue * scale / 100 + base;
            green = green * scale / 100 + base;
            red = red * scale / 100 + base;
            //printf("red: orig %u base %u scale %u%% result %u\n", val&0xff,base,scale,red);
            val = (blue << 16) | (green << 8) | red;
            noise_textures[tex][i] = val;
        }
    }
}



/*
 * Output buffering
 *
 * We buffer the output ourselves in an attempt to prevent flickering.
 */

static char* buffer;
static size_t buffer_capacity;
static size_t buffer_count;

static void buffer_append(const char* bytes, size_t count) {
    size_t total = buffer_count + count;
    if (total > buffer_capacity) {
        while (total > buffer_capacity)
            buffer_capacity *= 2;
        buffer = realloc(buffer, buffer_capacity);
        if (buffer == NULL) {
            fprintf(stderr, "Out of memory re-allocating output buffer!\n");
            abort();
        }
    }
    memcpy(buffer + buffer_count, bytes, count);
    buffer_count = total;
}

#define buffer_append_literal(str) buffer_append(str, sizeof(str) - 1)

static void buffer_append_cstr(const char* bytes) {
    buffer_append(bytes, strlen(bytes));
}

static size_t buffer_append_format(const char* format, ...) {
    char local[256];
    va_list args;
    va_start(args, format);
    size_t bytes = vsnprintf(local, sizeof(local), format, args);
    va_end(args);
    buffer_append(local, bytes);
    return bytes;
}

static void buffer_append_pad(size_t actual, size_t desired) {
    while (actual++ < desired)
        buffer_append(" ", 1);
}

static void buffer_append_byte_decimal(uint32_t value) {
    buffer_append_cstr(u8_to_str[value]);
}



/*
 * Colors
 */

/*
 * Weights for calculating brightness of pixel, as a fraction of 255. These
 * numbers are from ITU BT.709.
 */
#define LUMA_WEIGHT_RED 54      // 0.2126
#define LUMA_WEIGHT_GREEN 183   // 0.7152
#define LUMA_WEIGHT_BLUE 18     // 0.0722

#define LUMA_P(x) \
    (x[0] * LUMA_WEIGHT_BLUE + x[1] * LUMA_WEIGHT_GREEN + x[2] * LUMA_WEIGHT_RED)

#define LUMA(r, g, b) \
    (b * LUMA_WEIGHT_BLUE + g * LUMA_WEIGHT_GREEN + r * LUMA_WEIGHT_RED)

/*
 * Weights for calculating color differences, as a fraction of 16.
 */
#define DIFF_WEIGHT_RED 5
#define DIFF_WEIGHT_GREEN 7
#define DIFF_WEIGHT_BLUE 4

/**
 * Searches for the closest color in the given colors array and returns its code.
 */
static int color_search(const uint8_t* colors, size_t color_count,
        int red, int green, int blue)
{
    int best_code = 0;
    int best_error = INT_MAX;
    const uint8_t* end = colors + color_count * 4;
    while (colors != end) {
        int rc = colors[1];
        int gc = colors[2];
        int bc = colors[3];
        int rcd = ((red - rc) * DIFF_WEIGHT_RED);
        int gcd = ((green - gc) * DIFF_WEIGHT_GREEN);
        int bcd = ((blue - bc) * DIFF_WEIGHT_BLUE);
        int error = rcd * rcd + gcd * gcd + bcd * bcd;
        //int error = abs(rcd) + abs(gcd) + abs(bcd);
        if (error < best_error) {
            best_error = error;
            best_code = *colors;
        }
        colors += 4;
    }
    return best_code;
}

/**
 * Returns the 3-bit ANSI foreground color code for the given color.
 *
 * For background colors, add 10.
 */
static int color_3bit(int red, int green, int blue) {

    // These are not the real colors; we've multiplied the VGA palette by 1.5.
    // Really we just want as much contrast as possible. We send bold as well
    // to try to brighten the screen.
    static const uint8_t colors[] = {
        // code     red    green     blue
            30,       0,       0,       0,
            31,     255,       0,       0,
            32,       0,     255,       0,
            33,     255,     128,       0,
            34,       0,       0,     255,
            35,     255,       0,     255,
            36,       0,     255,     255,
            37,     255,     255,     255,
    };

    return color_search(colors, sizeof(colors) / 4, red, green, blue);
}

/**
 * Returns the 4-bit ANSI foreground color code for the given color.
 *
 * For background colors, add 10.
 */
static int color_4bit(int red, int green, int blue) {

    // These are the real VGA colors.
    static const uint8_t colors[] = {
        // code     red    green     blue
            30,       0,       0,       0,
            31,     170,       0,       0,
            32,       0,     170,       0,
            33,     170,      85,       0,  // dark orange, not dark yellow
            34,       0,       0,     170,
            35,     170,       0,     170,
            36,       0,     170,     170,
            37,     170,     170,     170,
            90,      85,      85,      85,
            91,     255,      85,      85,
            92,      85,     255,      85,
            93,     255,     255,      85,
            94,      85,      85,     255,
            95,     255,      85,     255,
            96,      85,     255,     255,
            97,     255,     255,     255,
    };
    return color_search(colors, sizeof(colors) / 4, red, green, blue);
}

static inline int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * Returns the 8-bit ANSI color code for the given color.
 *
 * We calculate the nearest color from the 6x6x6 color cube and the nearest
 * of the 24 grayscale colors. We use whichever is closer, except we bias
 * towards grayscale for darker areas.
 *
 * TODO fix, bias towards grayscale shouldn't be necessary. if we do bias we
 * should just brighten up the darkest colors. maybe even just adding 15 to
 * each color channel, or scale 0-255 to say 20-255, to bias towards having any
 * color at all. not sure how to handle error calculation in this case
 *
 * TODO another option is to select between grayscale and color based on how
 * much color there is. e.g. (r-b)^2+(r-g)^2+(b-g)^2 is amount of color, have
 * some threshold to use the color channels. probably no point if we do the
 * error calculation correctly
 *
 * TODO the ends of the 24-shade grayscale palette are #080808 and #eeeeee. We
 * don't need currently need any special cases for this though because #000000
 * and #ffffff exist in the color cube... but we might need a special case if
 * we overly bias it.
 */
static int color_8bit(int red, int green, int blue) {
    red = clamp(red, 0, 255);
    green = clamp(green, 0, 255);
    blue = clamp(blue, 0, 255);

    // TODO add x/y coordinate parameters and sample blue noise

    // TODO maybe replace these divisions with a multiplication and shift (good
    // compilers can do this for us but toy compilers won't)


    // TODO these next two blocks don't do a very good job of picking colors.
    // e.g. rgb of 127 gives us the grayscale block 112, not 128. needs fixing.

    // calculate 6x6x6 color cube
    // TODO color cube colors are not linear! maybe the simplest and fastest
    // way to do this is to manually create a lookup table to convert each
    // channel. we could put the bias right in the table.
    int r6 = red / 43;
    int g6 = green / 43;
    int b6 = blue / 43;
    int r6d = ((red - r6 * 43) * DIFF_WEIGHT_RED);
    int g6d = ((green - g6 * 43) * DIFF_WEIGHT_GREEN);
    int b6d = ((blue - b6 * 43) * DIFF_WEIGHT_BLUE);
    int error6 = r6d * r6d + g6d * g6d + b6d * b6d;
    //int error6 = abs(r6d) + abs(g6d) + abs(b6d);
    //printf("\033[0m   %i %i %i\n", r6d,g6d,b6d);

    // calculate grayscale
    // TODO these calculations are wrong, colors range from #08 to #ee.
    // probably should also just use a lookup table
    int luma = LUMA(red, green, blue);
    int gray = (luma * 24) >> 16;
    int gray256 = gray * 256 / 24;
    int rgd = ((red - gray256) * DIFF_WEIGHT_RED);
    int ggd = ((green - gray256) * DIFF_WEIGHT_GREEN);
    int bgd = ((blue - gray256) * DIFF_WEIGHT_BLUE);
    int errorg = rgd * rgd + ggd * ggd + bgd * bgd;
    //int errorg = abs(rgd) + abs(ggd) + abs(bgd);
    //printf("\033[0m   %i %i %i\n", rgd,ggd,bgd);

    // if luma is small, reduce the grayscale error
    /*
    if (luma < 16384) {
        errorg *= luma >> 8;
        errorg >>= 4;
    }
    */

    // TODO this comment is wrong, we DON'T want to bias above the 1/8 or 1/4 threshold
        // Bias grayscale based on luma. This decreases errorg in the bottom 1/8 of
        // luma and increases it otherwise. This ensures dark areas are playable
        // while basically never using grayscale in brighter ares (unless the color
        // is actually gray.)

    //printf("\033[0m%u %u   %u %u %u   %u    %u\n", error6, errorg,r6,g6,b6,gray,luma);

    return ((error6 < errorg)) ?
            16 + b6 + g6 * 6 + r6 * 36 : // color cube
            232 + gray;                  // grayscale
}

// Outputs a background color.
static void output_bg_color(int x, int y, int red, int green, int blue) {
    if (noise_enabled) {
        uint32_t noise_color = NOISE_SAMPLE(x, y);
        red += (noise_color >> 16 & 0xff) - 128;
        green += ((noise_color >> 8) & 0xff) - 128;
        blue += ((noise_color) & 0xff) - 128;

        /*
        red = ((noise_color >> 16) & 0xff);
        green = ((noise_color >> 8) & 0xff);
        blue = ((noise_color) & 0xff);
        */
    }

    switch (cli_colors) {
        case cli_colors_24bit:
            buffer_append_format("\033[48;2;%u;%u;%um", red, green, blue);
            return;
        case cli_colors_8bit:
            buffer_append_format("\033[48;5;%um", color_8bit(red, green, blue));
            return;
        case cli_colors_4bit:
            buffer_append_format("\033[%um", 10 + color_4bit(red, green, blue));
            return;
        case cli_colors_3bit:
            buffer_append_format("\033[%um", 10 + color_3bit(red, green, blue));
            return;
    }
}

// Outputs both background and foreground colors.
static void output_colors(
        int x, int y,
        int fg_red, int fg_green, int fg_blue,
        int bg_red, int bg_green, int bg_blue)
{
    // TODO this applies noise per character which is not what we should be
    // doing. We would get much better noise quality if we applied it per
    // pixel.
    if (noise_enabled) {
        uint32_t noise_color = NOISE_SAMPLE(x, y);

        fg_red += ((noise_color >> 16) & 0xff) - 128;
        bg_red += ((noise_color >> 16) & 0xff) - 128;
        fg_green += ((noise_color >> 8) & 0xff) - 128;
        bg_green += ((noise_color >> 8) & 0xff) - 128;
        fg_blue += ((noise_color) & 0xff) - 128;
        bg_blue += ((noise_color) & 0xff) - 128;

        /*
        fg_red = (noise_color & 0xff);
        bg_red = (noise_color & 0xff);
        fg_green = ((noise_color >> 8) & 0xff);
        bg_green = ((noise_color >> 8) & 0xff);
        fg_blue = ((noise_color >> 16) & 0xff);
        bg_blue = ((noise_color >> 16) & 0xff);
        */
    }

    #define INT_NAME(x) x  // TODO remove INT_NAME()

    char buf[256];

    switch (cli_colors) {
        case cli_colors_24bit: {
            char* p = mempcpy(buf, "\033[38;2;", sizeof("\033[38;2;") - 1);
            p = stpcpy(p, u8_to_str[fg_red]);
            *p++ = ';';
            p = stpcpy(p, u8_to_str[fg_green]);
            *p++ = ';';
            p = stpcpy(p, u8_to_str[fg_blue]);
            p = mempcpy(p, "m\033[48;2;", sizeof("m\033[48;2;") - 1);
            p = stpcpy(p, u8_to_str[bg_red]);
            *p++ = ';';
            p = stpcpy(p, u8_to_str[bg_green]);
            *p++ = ';';
            p = stpcpy(p, u8_to_str[bg_blue]);
            *p++ = 'm';
            buffer_append(buf, p - buf);

            // TODO onramp printf() is too slow
            /*
            buffer_append_format("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um",
                    INT_NAME(fg_red), INT_NAME(fg_green), INT_NAME(fg_blue),
                    INT_NAME(bg_red), INT_NAME(bg_green), INT_NAME(bg_blue));
                    */
            return;
        }
        case cli_colors_8bit:
            buffer_append_format("\033[38;5;%um\033[48;5;%um",
                    color_8bit(INT_NAME(fg_red), INT_NAME(fg_green), INT_NAME(fg_blue)),
                    color_8bit(INT_NAME(bg_red), INT_NAME(bg_green), INT_NAME(bg_blue)));
            return;
        case cli_colors_4bit:
            buffer_append_format("\033[%u;%um",
                    color_4bit(INT_NAME(fg_red), INT_NAME(fg_green), INT_NAME(fg_blue)),
                    color_4bit(INT_NAME(bg_red), INT_NAME(bg_green), INT_NAME(bg_blue)) + 10);
            return;
        case cli_colors_3bit:
            buffer_append_format("\033[%u;%um",
                    color_3bit(INT_NAME(fg_red), INT_NAME(fg_green), INT_NAME(fg_blue)),
                    color_3bit(INT_NAME(bg_red), INT_NAME(bg_green), INT_NAME(bg_blue)) + 10);
            return;
    }
}

static void output_newline(void) {
    buffer_append_literal("\033[0m\n");
}



/*
 * Rendering
 */

static void start_row() {
    DOOMCLI_READ_INPUT();
    if (cli_colors == cli_colors_3bit) {
        // send bold, hopefully the terminal interprets it as bright
        buffer_append_literal("\033[1m");
    }
}

static void draw_space() {
    uint32_t* dest_pixel = dest_buffer;
    for (int y = 0; y < dest_height; ++y) {
        start_row();
        for (int x = 0; x < dest_width; ++x) {
            uint8_t* pixel = (uint8_t*)dest_pixel;
            output_bg_color(x, y, pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);
            buffer_append_literal(" ");
            ++dest_pixel;
        }
        output_newline();
    }
}

static void draw_half() {
    uint32_t* top = dest_buffer;
    uint32_t* bot = dest_buffer + dest_width;

    for (int y = 0; y < dest_height; y += 2) {
        start_row();
        for (int x = 0; x < dest_width; ++x) {
            //printf("\033[48;2;%u;%u;%um ", pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);

            output_colors(x, y,
                    ((uint8_t*)top)[2], ((uint8_t*)top)[1], ((uint8_t*)top)[0],
                    ((uint8_t*)bot)[2], ((uint8_t*)bot)[1], ((uint8_t*)bot)[0]);
            if (cli_mode == cli_mode_half) {
                buffer_append_literal(UPPER_HALF);
            }

            ++top;
            ++bot;
        }

        output_newline();
        top += dest_width;
        bot += dest_width;
    }
}

static void draw_quadrant() {
    uint32_t* top = dest_buffer;
    uint32_t* bot = dest_buffer + dest_width;

    for (int y = 0; y < dest_height; y += 2) {
        start_row();
        for (int x = 0; x < dest_width; x += 2) {

            // get pixels
            uint8_t* tl = (uint8_t*)&top[0];
            uint8_t* tr = (uint8_t*)&top[1];
            uint8_t* bl = (uint8_t*)&bot[0];
            uint8_t* br = (uint8_t*)&bot[1];
            /*
            printf("%u %u %u   ", tl[0], tl[1], tl[2]);
            printf("%u %u %u\n", tr[0], tr[1], tr[2]);
            printf("%u %u %u   ", bl[0], bl[1], bl[2]);
            printf("%u %u %u\n", br[0], br[1], br[2]);
            */

            // calculate average luma
            uint32_t l_tl = LUMA_P(tl);
            uint32_t l_tr = LUMA_P(tr);
            uint32_t l_bl = LUMA_P(bl);
            uint32_t l_br = LUMA_P(br);
            //printf("luma %u %u %u %u\n", l_tl, l_tr, l_bl, l_br);
            uint32_t l_avg = (l_tl + l_tr + l_bl + l_br) >> 2;
            //printf("luma avg %u\n", l_avg);
            //l_avg=127*256;

            // foreground is bright, background is dark
            int index =
                 (l_tl > l_avg) |
                ((l_tr > l_avg) << 1) |
                ((l_bl > l_avg) << 2) |
                ((l_br > l_avg) << 3);

            // if all bits are set, we use only background (space)
            // TODO fix this later, need to also clear luma or check bits in below conditionals
            /*if (index == 0b1111) {
                index = 0;
            }*/

        //fputs("\033[0m", stdout);
            //printf("index %i char %s\n", index, quadrants[index]);

            /*
            bool on_tl = l_tl > l_avg;
            bool on_tr = l_tr > l_avg;
            bool on_bl = l_bl > l_avg;
            bool on_br = l_br > l_avg;
            */

            // choose foreground and background colors
            uint32_t fg_red = 0;
            uint32_t fg_green = 0;
            uint32_t fg_blue = 0;
            int fg_count = 0;
            uint32_t bg_red = 0;
            uint32_t bg_green = 0;
            uint32_t bg_blue = 0;
            int bg_count = 0;

            /*
            uint8_t blue  = (uint8_t)((tl[0] + tr[0] + bl[0] + br[0]) >> 2);
            uint8_t green = (uint8_t)((tl[1] + tr[1] + bl[1] + br[1]) >> 2);
            uint8_t red   = (uint8_t)((tl[2] + tr[2] + bl[2] + br[2]) >> 2);
            */

            if (l_tl > l_avg) {
                fg_blue += tl[0];
                fg_green += tl[1];
                fg_red += tl[2];
                ++fg_count;
            } else {
                bg_blue += tl[0];
                bg_green += tl[1];
                bg_red += tl[2];
                ++bg_count;
            }

            if (l_tr > l_avg) {
                fg_blue += tr[0];
                fg_green += tr[1];
                fg_red += tr[2];
                ++fg_count;
            } else {
                bg_blue += tr[0];
                bg_green += tr[1];
                bg_red += tr[2];
                ++bg_count;
            }

            if (l_bl > l_avg) {
                fg_blue += bl[0];
                fg_green += bl[1];
                fg_red += bl[2];
                ++fg_count;
            } else {
                bg_blue += bl[0];
                bg_green += bl[1];
                bg_red += bl[2];
                ++bg_count;
            }

            if (l_br > l_avg) {
                fg_blue += br[0];
                fg_green += br[1];
                fg_red += br[2];
                ++fg_count;
            } else {
                bg_blue += br[0];
                bg_green += br[1];
                bg_red += br[2];
                ++bg_count;
            }

            if (fg_count > 0) {
                fg_blue /= fg_count;
                fg_green /= fg_count;
                fg_red /= fg_count;
            }

            if (bg_count > 0) {
                bg_blue /= bg_count;
                bg_green /= bg_count;
                bg_red /= bg_count;
            }




            //printf("\033[48;2;%u;%u;%um ", pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);

            /*
            printf("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um" UPPER_HALF,
                    ((uint8_t*)top)[2], ((uint8_t*)top)[1], ((uint8_t*)top)[0],
                    ((uint8_t*)bot)[2], ((uint8_t*)bot)[1], ((uint8_t*)bot)[0]);
                    */

            if (index == 0) {
                output_bg_color(x, y, bg_red, bg_green, bg_blue);
                buffer_append_literal(" ");
            } else {
                output_colors(x, y,
                        fg_red, fg_green, fg_blue,
                        bg_red, bg_green, bg_blue);
                buffer_append_cstr(quadrants[index]);
            }

            top += 2;
            bot += 2;
        }

        output_newline();
        top += dest_width;
        bot += dest_width;
    }
}

static void draw_sextant_bw() {
//printf("%s %i  draw sextant\n",__func__, DG_GetTicksMs());
    uint32_t* top = dest_buffer;
    uint32_t* mid = top + dest_width;
    uint32_t* bot = mid + dest_width;

    for (int y = 0; y < dest_height; y += 3) {
        start_row();
        for (int x = 0; x < dest_width; x += 2) {

            // get pixels
            uint8_t* tl = (uint8_t*)&top[0];
            uint8_t* tr = (uint8_t*)&top[1];
            uint8_t* ml = (uint8_t*)&mid[0];
            uint8_t* mr = (uint8_t*)&mid[1];
            uint8_t* bl = (uint8_t*)&bot[0];
            uint8_t* br = (uint8_t*)&bot[1];

            // calculate luma
            uint32_t l_tl = LUMA_P(tl) >> 8;
            uint32_t l_tr = LUMA_P(tr) >> 8;
            uint32_t l_ml = LUMA_P(ml) >> 8;
            uint32_t l_mr = LUMA_P(mr) >> 8;
            uint32_t l_bl = LUMA_P(bl) >> 8;
            uint32_t l_br = LUMA_P(br) >> 8;

            if (noise_enabled) {
                // We use only the red channel of noise.
                //printf("pixel x %i y %i luma %i noise raw %i noise actual %i\n", x,y,l_tl,NOISE_SAMPLE(x    , y    )&0xff,(NOISE_SAMPLE(x    , y    ) & 0xff) - 128);
                l_tl = clamp(l_tl + (NOISE_SAMPLE(x    , y    ) & 0xff) - 128, 0, 255);
                l_tr = clamp(l_tr + (NOISE_SAMPLE(x + 1, y    ) & 0xff) - 128, 0, 255);
                l_ml = clamp(l_ml + (NOISE_SAMPLE(x    , y + 1) & 0xff) - 128, 0, 255);
                l_mr = clamp(l_mr + (NOISE_SAMPLE(x + 1, y + 1) & 0xff) - 128, 0, 255);
                l_bl = clamp(l_bl + (NOISE_SAMPLE(x    , y + 2) & 0xff) - 128, 0, 255);
                l_br = clamp(l_br + (NOISE_SAMPLE(x + 1, y + 2) & 0xff) - 128, 0, 255);
            }

            // calculate index
            //printf("%i %i %i %i %i %i\n", l_tl,l_tr,l_ml,l_mr,l_bl,l_br);
            const uint32_t threshold = 127;
            int index =
                ((l_tl > threshold)     ) |
                ((l_tr > threshold) << 1) |
                ((l_ml > threshold) << 2) |
                ((l_mr > threshold) << 3) |
                ((l_bl > threshold) << 4) |
                ((l_br > threshold) << 5);

            if (cli_colors == cli_colors_light) {
                index = (~index) & 0x3f;
            }

            buffer_append_cstr(sextants[index]);

            top += 2;
            mid += 2;
            bot += 2;
        }

        output_newline();
        top += dest_width << 1;
        mid += dest_width << 1;
        bot += dest_width << 1;
    }
}

static void draw_sextant() {
//printf("%s %i  draw sextant\n",__func__, DG_GetTicksMs());
    uint32_t* top = dest_buffer;
    uint32_t* mid = top + dest_width;
    uint32_t* bot = mid + dest_width;

    for (int y = 0; y < dest_height; y += 3) {
        start_row();
        for (int x = 0; x < dest_width; x += 2) {

            // get pixels
            uint8_t* tl = (uint8_t*)&top[0];
            uint8_t* tr = (uint8_t*)&top[1];
            uint8_t* ml = (uint8_t*)&mid[0];
            uint8_t* mr = (uint8_t*)&mid[1];
            uint8_t* bl = (uint8_t*)&bot[0];
            uint8_t* br = (uint8_t*)&bot[1];
            /*
            printf("%u %u %u   ", tl[0], tl[1], tl[2]);
            printf("%u %u %u\n", tr[0], tr[1], tr[2]);
            printf("%u %u %u   ", bl[0], bl[1], bl[2]);
            printf("%u %u %u\n", br[0], br[1], br[2]);
            */

            // calculate average luma
            uint32_t l_tl = LUMA_P(tl);
            uint32_t l_tr = LUMA_P(tr);
            uint32_t l_ml = LUMA_P(ml);
            uint32_t l_mr = LUMA_P(mr);
            uint32_t l_bl = LUMA_P(bl);
            uint32_t l_br = LUMA_P(br);
            //printf("luma %u %u %u %u\n", l_tl, l_tr, l_bl, l_br);
            uint32_t l_avg = (l_tl + l_tr + l_ml + l_mr + l_bl + l_br) / 6;
            //printf("luma avg %u\n", l_avg);
            //l_avg=127*256;

            // foreground is bright, background is dark
            int index = 0;
            /*
            int index =
                 (l_tl > l_avg) |
                ((l_tr > l_avg) << 1) |
                ((l_ml > l_avg) << 2) |
                ((l_mr > l_avg) << 3) |
                ((l_bl > l_avg) << 4) |
                ((l_br > l_avg) << 5);
                */

            // if all bits are set, we use only background (space)
            // TODO fix this later, need to also clear luma or check bits in below conditionals
            /*if (index == 0b111111) {
                index = 0;
            }*/

        //fputs("\033[0m", stdout);
            //printf("index %i char %s\n", index, quadrants[index]);

            /*
            bool on_tl = l_tl > l_avg;
            bool on_tr = l_tr > l_avg;
            bool on_bl = l_bl > l_avg;
            bool on_br = l_br > l_avg;
            */

            // choose foreground and background colors
            uint32_t fg_red = 0;
            uint32_t fg_green = 0;
            uint32_t fg_blue = 0;
            int fg_count = 0;
            uint32_t bg_red = 0;
            uint32_t bg_green = 0;
            uint32_t bg_blue = 0;
            int bg_count = 0;

            /*
            uint8_t blue  = (uint8_t)((tl[0] + tr[0] + bl[0] + br[0]) >> 2);
            uint8_t green = (uint8_t)((tl[1] + tr[1] + bl[1] + br[1]) >> 2);
            uint8_t red   = (uint8_t)((tl[2] + tr[2] + bl[2] + br[2]) >> 2);
            */

            if (l_tl > l_avg) {
                fg_blue += tl[0];
                fg_green += tl[1];
                fg_red += tl[2];
                ++fg_count;
                index |= 1;
            } else {
                bg_blue += tl[0];
                bg_green += tl[1];
                bg_red += tl[2];
                ++bg_count;
            }

            if (l_tr > l_avg) {
                fg_blue += tr[0];
                fg_green += tr[1];
                fg_red += tr[2];
                ++fg_count;
                index |= 2;
            } else {
                bg_blue += tr[0];
                bg_green += tr[1];
                bg_red += tr[2];
                ++bg_count;
            }

            if (l_ml > l_avg) {
                fg_blue += ml[0];
                fg_green += ml[1];
                fg_red += ml[2];
                ++fg_count;
                index |= 4;
            } else {
                bg_blue += ml[0];
                bg_green += ml[1];
                bg_red += ml[2];
                ++bg_count;
            }

            if (l_mr > l_avg) {
                fg_blue += mr[0];
                fg_green += mr[1];
                fg_red += mr[2];
                ++fg_count;
                index |= 8;
            } else {
                bg_blue += mr[0];
                bg_green += mr[1];
                bg_red += mr[2];
                ++bg_count;
            }

            if (l_bl > l_avg) {
                fg_blue += bl[0];
                fg_green += bl[1];
                fg_red += bl[2];
                ++fg_count;
                index |= 16;
            } else {
                bg_blue += bl[0];
                bg_green += bl[1];
                bg_red += bl[2];
                ++bg_count;
            }

            if (l_br > l_avg) {
                fg_blue += br[0];
                fg_green += br[1];
                fg_red += br[2];
                ++fg_count;
                index |= 32;
            } else {
                bg_blue += br[0];
                bg_green += br[1];
                bg_red += br[2];
                ++bg_count;
            }

            if (fg_count > 0) {
                fg_blue /= fg_count;
                fg_green /= fg_count;
                fg_red /= fg_count;
            }

            if (bg_count > 0) {
                bg_blue /= bg_count;
                bg_green /= bg_count;
                bg_red /= bg_count;
            }




            //printf("\033[48;2;%u;%u;%um ", pixel[2], pixel[1], pixel[0]);
            //printf("\033[48;5;%um ", 16 + (pixel[0] / 43) + (pixel[1] / 43) * 6 + (pixel[2] / 43) * 36);

            /*
            printf("\033[38;2;%u;%u;%um\033[48;2;%u;%u;%um" UPPER_HALF,
                    ((uint8_t*)top)[2], ((uint8_t*)top)[1], ((uint8_t*)top)[0],
                    ((uint8_t*)bot)[2], ((uint8_t*)bot)[1], ((uint8_t*)bot)[0]);
                    */

//if (x < 10){
            if (index == 0) {
                output_bg_color(x, y, bg_red, bg_green, bg_blue);
                buffer_append_literal(" ");
            } else {
                //buffer_append_format("\033[0m%u",index);
                output_colors(x, y,
                        fg_red, fg_green, fg_blue,
                        bg_red, bg_green, bg_blue);
                buffer_append_cstr(sextants[index]);
            }
//}

            top += 2;
            mid += 2;
            bot += 2;
        }

        output_newline();
        top += dest_width << 1;
        mid += dest_width << 1;
        bot += dest_width << 1;
    }
//printf("%s %i  done\n",__func__, DG_GetTicksMs());
}



/*
 * Callbacks
 */

static void setup_io(void) {

    // TODO need to set up signals to reset termios

    // unbuffered
    setvbuf(stdin, NULL, _IONBF, BUFSIZ);

    // non-blocking
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);

#ifndef DEBUG_FIXED_TICKRATE
    // non-canonical, no echo
    struct termios termios;
    tcgetattr(STDIN_FILENO, &termios);
    termios.c_lflag &= ~(ECHO | ICANON);
    //cfmakeraw(&termios);
    tcsetattr(STDIN_FILENO, TCSANOW, &termios);
#endif

}

static void parse_cli_options() {
    int arg = M_CheckParmWithArgs("-charset", 1);
    if (arg)
    {
        const char* charset = myargv[arg + 1];
        if (0 == strcmp(charset, "sextant")) {
            cli_mode = cli_mode_sextant;
        } else if (0 == strcmp(charset, "quadrant")) {
            cli_mode = cli_mode_quadrant;
        } else if (0 == strcmp(charset, "half")) {
            cli_mode = cli_mode_half;
        } else if (0 == strcmp(charset, "space")) {
            cli_mode = cli_mode_space;
        } else {
            fprintf(stderr, "Unrecognized charset option: \"%s\"\n", charset);
            abort();
        }
    }

    arg = M_CheckParmWithArgs("-color", 1);
    if (arg)
    {
        const char* color = myargv[arg + 1];
        if (0 == strcmp(color, "24bit")) {
            cli_colors = cli_colors_24bit;
        } else if (0 == strcmp(color, "8bit")) {
            cli_colors = cli_colors_8bit;
        } else if (0 == strcmp(color, "4bit")) {
            cli_colors = cli_colors_4bit;
        } else if (0 == strcmp(color, "3bit")) {
            cli_colors = cli_colors_3bit;
        } else if (0 == strcmp(color, "light")) {
            cli_colors = cli_colors_light;
        } else if (0 == strcmp(color, "dark")) {
            cli_colors = cli_colors_dark;
        } else {
            fprintf(stderr, "Unrecognized color option: \"%s\"\n", color);
            abort();
        }
    }

    arg = M_CheckParmWithArgs("-filter", 1);
    if (arg)
    {
        const char* filter = myargv[arg + 1];
        if (0 == strcmp(filter, "box")) {
            // TODO
        } else if (0 == strcmp(filter, "nearest")) {
            // TODO
        } else {
            // TODO
        }
        fprintf(stderr, "Filter option is not yet implemented.\n");
        abort();
    }

    arg = M_CheckParmWithArgs("-noise", 1);
    if (arg)
    {
        const char* noise = myargv[arg + 1];
        if (0 == strcmp(noise, "on")) {
            noise_enabled = true;
        } else if (0 == strcmp(noise, "off")) {
            noise_enabled = false;
        } else {
            fprintf(stderr, "Unrecognized noise option: \"%s\"\n", noise);
            abort();
        }
    }

    arg = M_CheckParmWithArgs("-noise-speed", 1);
    if (arg)
    {
        noise_speed = atoi(myargv[arg + 1]);
    }

    arg = M_CheckParmWithArgs("-noise-strength", 1);
    if (arg)
    {
        // TODO
        fprintf(stderr, "Noise strength option is not yet implemented.\n");
        abort();
    }

    arg = M_CheckParmWithArgs("-columns", 1);
    if (arg)
    {
        columns = atoi(myargv[arg + 1]);
    }

    if (cli_colors == cli_colors_dark || cli_colors == cli_colors_light) {
        if (cli_mode == cli_mode_space) {
            fprintf(stderr, "The space charset is incompatible with light and dark color modes.\n");
            abort();
        }
        if (cli_mode != cli_mode_sextant) {
            fprintf(stderr, "TODO dark/light color mode is only implemented for sextant charset.\n");
            abort();
        }
    }
}

void DG_Init()
{
    setup_io();

    parse_cli_options();

    buffer_capacity = 1024*1024;
    buffer_count = 0;
    buffer = malloc(buffer_capacity);
    if (buffer == NULL) {
        fprintf(stderr, "Out of memory allocating output buffer!\n");
        abort();
    }

    init_noise();

    dest_width = columns;
    switch (cli_mode) {
        case cli_mode_space:
        case cli_mode_half:
            // nothing
            break;
        case cli_mode_quadrant:
        case cli_mode_sextant:
            dest_width *= 2;
            break;
    }

    // We assume the terminal has a character aspect ratio of 4:9, and Doom is
    // intended to be rendered at a ratio of 4:3.
    switch (cli_mode) {
        case cli_mode_space:
            dest_height = dest_width * 12 / 36;
            break;
        case cli_mode_half:
            dest_height = dest_width * 24 / 36;
            dest_height &= ~1;
            break;
        case cli_mode_quadrant:
            dest_height = dest_width * 12 / 36;
            dest_height &= ~1;
            break;
        case cli_mode_sextant:
            dest_height = dest_width * 18 / 36;
            dest_height = dest_height / 3 * 3;
            break;
    }

    dest_buffer = malloc(sizeof(uint32_t) * dest_width * dest_height);


    // Send a synchronized output query. This will tell us whether the terminal
    // supports synchronized updates.
    //     https://gist.github.com/christianparpart/d8a62cc1ab659194337d73e399004036?permalink_comment_id=3946967
    // TODO we don't bother doing this right now because I'm too lazy to parse
    // the response. For now we just assume it's supported.
    //fputs("\033[?2026$p", stdout);
    synchronized_updates = true;

    /*
int red = 33;
int green = 64;
int blue = 0;

printf("\033[48;2;%u;%u;%um", red, green, blue);
printf(" ");
printf("\033[0m\n");

printf("\033[48;5;%um ", color_8bit(red, green, blue));

printf("\033[0m\n");
fflush(stdout);
abort();
*/
}


void DG_DrawFrame()
{
//return;
//printf("DG_DrawFrame() exiting\n");
//exit(0);
    if (noise_enabled) {
        uint32_t time = DG_GetTicksMs();
        if (time - noise_last_time > noise_speed) {
            noise_last_time = time;
            noise_current = (noise_current + 1) % noise_texture_count;
        }
    }

//printf("%s %i  resampling down\n",__func__, DG_GetTicksMs());
    // resample the frame down
    // TODO for now we just choose the nearest pixel, need to implement at least a box filter
    uint32_t* dest_pixel = dest_buffer;
    for (int y = 0; y < dest_height; ++y) {
        for (int x = 0; x < dest_width; ++x) {
            int sy = y * DOOMGENERIC_RESY / dest_height;
            int sx = x * DOOMGENERIC_RESX / dest_width;

            // old code letting doomgeneric build a 32-bit screenbuffer
            //*dest_pixel++ = DG_ScreenBuffer[sy * DOOMGENERIC_RESX + sx];

            // new code using palette directly
            *dest_pixel++ = *(uint32_t*)(colors + I_VideoBuffer[sy * SCREENWIDTH + sx]);
        }
    }

    // use synchronized updates if supported. append a newline in case it's not.
    if (synchronized_updates) {
        buffer_append_literal("\033[?2026h\n");
    }

    // clear the screen
    buffer_append_literal("\033[2J\033[1;1H");

    // hide the cursor
    buffer_append_literal("\033[?25l");

//printf("%s %i  drawing\n",__func__, DG_GetTicksMs());
    switch (cli_mode) {
        case cli_mode_space:
            draw_space();
            break;
        case cli_mode_half: // fallthrough
            draw_half();
            break;
        case cli_mode_quadrant:
            draw_quadrant();
            break;
        case cli_mode_sextant:
            if (cli_colors == cli_colors_dark || cli_colors == cli_colors_light)
                draw_sextant_bw();
            else
                draw_sextant();
            break;
    }

    // append statistics
    if (print_stats) {

        // collect data
        int current_time = stats_times[stats_next] = DG_GetTicksMs();
        stats_sizes[stats_next] = buffer_count;
        stats_next = (stats_next + 1) % stats_capacity;

        if (stats_count < stats_capacity) {
            // not enough frames to reliably calculate fps
            ++stats_count;
        } else {
            // print frame size
            int average_size = 0;
            for (int i = 0; i < stats_capacity; ++i)
                average_size += stats_sizes[i];
            average_size /= stats_capacity;
            size_t len = buffer_append_format("frame size: %zi B", average_size);
            buffer_append_pad(len, 25);

            // print frame rate
            int fps = 1000 * stats_capacity / (current_time - stats_times[stats_next]);
            len = buffer_append_format("frame rate: %i FPS", fps);
            buffer_append_pad(len, 25);

            // print data rate
            int data_rate = fps * average_size / 1000;
            len = buffer_append_format("data rate: %i kB/s", data_rate);
            buffer_append_pad(len, 25);
        }
        buffer_append("\n", 1);

        buffer_append_format("key repeat delay: %i ms    key repeat rate: %i ms\n", key_repeat_delay, key_repeat_rate);
    }

    // show the cursor
    // TODO trap ctrl+c and send this and the color reset code, this is real annoying
    buffer_append_literal("\033[?25h");

    // done the update
    if (synchronized_updates) {
        buffer_append_literal("\033[?2026l");
    }

#if 1
//printf("%s %i  writing\n",__func__, DG_GetTicksMs());
    // write the buffer to standard output
    fflush(stdout);
    char* p = buffer;
    size_t remaining = buffer_count;
    while (remaining > 0) {
        ssize_t step = write(STDOUT_FILENO, p, remaining);
        if (step <= 0) {
            if (step == 0 || errno == EWOULDBLOCK || errno == EAGAIN) {
                usleep(1);
                continue;
            }
            fprintf(stderr, "Failed to write output data!\n");
            abort();
        }
        remaining -= step;
        p += step;
    }
#endif

    buffer_count = 0;
//printf("%s %i  done\n",__func__, DG_GetTicksMs());
}

// copied from doomgeneric_xlib.c
void DG_SleepMs(uint32_t ms)
{
    #ifdef DEBUG_FIXED_TICKRATE
    return;
    #endif
    usleep (ms * 1000);
}

// copied from doomgeneric_xlib.c
uint32_t DG_GetTicksMs()
{
    #ifdef DEBUG_FIXED_TICKRATE
    static int callCount;
    ++callCount;
    if (callCount == 2000) exit(0);
    return callCount*10;
    #endif

    #ifdef __onramp__
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    #endif

    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000); /* return milliseconds */
}

static void add_key_measurement(uint32_t delay, uint32_t rate) {
    //printf("add_key_measurement() delay %u rate %u\n", delay, rate);
    if (key_measure_count < KEY_MEASURE_CAPACITY)
        ++key_measure_count;
    //printf("next %i\n", key_measure_next);
    key_measure_delays[key_measure_next] = delay;
    key_measure_rates[key_measure_next] = rate;
    key_measure_next = (key_measure_next + 1) % KEY_MEASURE_CAPACITY;

    // We need to reduce these arrays to a single value to use for key repeat
    // rate and delay. We don't want to use the mean because this will give
    // outliers too much influence, and we don't want to use the median because
    // it's a bit cumbersome to implement (especially since Onramp's qsort()
    // isn't enabled yet.) Instead we do something much simpler: we calculate
    // the mean, then choose the closest value in the array to the mean.

    uint32_t mean_delay = 0;
    uint32_t mean_rate = 0;
    for (size_t i = 0; i < key_measure_count; ++i) {
        mean_delay += key_measure_delays[i];
        mean_rate += key_measure_rates[i];
    }
    //printf("mean_delay %i\n", mean_delay);
    mean_delay /= key_measure_count;
    mean_rate /= key_measure_count;

    uint32_t best_delay = 0;
    uint32_t best_delay_error = 999999999;
    uint32_t best_rate = 0;
    uint32_t best_rate_error = 999999999;
    for (size_t i = 0; i < key_measure_count; ++i) {
        uint32_t delay = key_measure_delays[i];
        uint32_t rate = key_measure_rates[i];
        //printf("delay %i %i %i\n", delay, mean_delay, abs(delay - mean_delay));
        if (abs(delay - mean_delay) < best_delay_error) {
            best_delay = delay;
            best_delay_error = abs(delay - mean_delay);
        }
        if (abs(rate - mean_rate) < best_rate_error) {
            best_rate = rate;
            best_rate_error = abs(rate - mean_rate);
        }
    }

    if (best_delay > KEY_REPEAT_DELAY_MAX) best_delay = KEY_REPEAT_DELAY_MAX;
    if (best_rate > KEY_REPEAT_RATE_MAX) best_rate = KEY_REPEAT_RATE_MAX;

    //printf("add_key_measurement() setting new values: delay %u rate %u\n", best_delay, best_rate);
    key_repeat_delay = best_delay;
    key_repeat_rate = best_rate;
}

static void detect_key_repeat(int key) {
    keyinfo_t* keyinfo = &keyinfos[key];
    if (keyinfo->time_count != TIME_CAPACITY)
        return;
    int n = keyinfo->time_next;

    // To look for repeats, we look at the time delta between all recorded
    // keypresses (except the first to account for the repeat delay.) If the
    // variance is low enough, we consider it to be a repeat.

    // calculate the mean
    uint32_t mean = 0;
    for (size_t i = 1; i < TIME_CAPACITY - 1; ++i) {
        uint32_t delta =
            keyinfo->time[(n + i + 1) % TIME_CAPACITY] -
            keyinfo->time[(n + i    ) % TIME_CAPACITY];
        //printf("  delta %i\n", delta);
        mean += delta;
    }
    mean /= TIME_CAPACITY - 2;
    //printf("mean %i\n", mean);

    // calculate the variance
    uint32_t variance = 0;
    for (size_t i = 1; i < TIME_CAPACITY - 1; ++i) {
        uint32_t delta =
            keyinfo->time[(n + i + 1) % TIME_CAPACITY] -
            keyinfo->time[(n + i    ) % TIME_CAPACITY];
        uint32_t error = abs(delta - mean);
        //printf("  error %i\n", error);
        variance += error;
    }
    //printf("variance %i\n", variance);

    if (variance > key_repeat_threshold * (TIME_CAPACITY - 2)) {
        //printf("not a repeat.\n");
        keyinfo->detected_repeat = false;
        return;
    }

    // We've detected a repeat! The mean delta is the key repeat rate. The
    // delta after the first keypress could be the repeat delay.
    uint32_t delay =
        keyinfo->time[(n + 1) % TIME_CAPACITY] -
        keyinfo->time[(n    ) % TIME_CAPACITY];
    uint32_t rate = mean;
    //printf("detected repeat! delay %u rate %u\n", delay, rate);

    // We only want to add it to our measurements when first detected. (We need
    // to avoid extra detections because the delay time will get overwritten by
    // another repeat.)
    if (!keyinfo->detected_repeat) {
        add_key_measurement(delay, rate);
        keyinfo->detected_repeat = true;
    }
}

/**
 * Handles a keypress.
 *
 * If duplicate is true, this is for a redundant key (e.g. key 'z' is both the
 * letter 'z' and the fire button) so it should not contribute twice to key
 * repeat estimation.
 */
static void keypress(int key, bool duplicate) {
    //printf("keypress %i dup %i\n", key, duplicate);
    keyinfo_t* keyinfo = &keyinfos[key];

    // insert the press time
    keyinfo->time[keyinfo->time_next] = DG_GetTicksMs();
    keyinfo->time_next = (keyinfo->time_next + 1) % TIME_CAPACITY;
    if (keyinfo->time_count < TIME_CAPACITY)
        ++keyinfo->time_count;

    // see if we can detect a key repeat
    if (!duplicate) {
        detect_key_repeat(key);
    }

    // handle state
    bool press = false;
    switch (keyinfo->state) {
        case keystate_off:
            press = true;
            keyinfo->state = keystate_down;
            break;
        // TODO in state down or wait, if time doesn't match key repeat, should
        // send both up and down events and go back to down state, so user
        // tapping isn't treated as repeat
        case keystate_wait:
            press = true; // fallthrough
        case keystate_down:
            keyinfo->state = keystate_repeat;
            break;
        case keystate_repeat:
            break;
    }

    // TODO maybe add some acceleration to the turn speed based on repeat
    // delay? or just turn more slowly outside of state repeat? so it's still
    // possible to do some precision aiming

    // if the key wasn't already down, add the event to the queue
    if (press) {
        keybuffer[keybuffer_write].pressed = true;
        keybuffer[keybuffer_write].key = key;
        keybuffer_write = (keybuffer_write + 1) % KEYBUFFER_CAPACITY;
    }
}

// Handle an input byte that isn't part of an escape sequence
static void handle_input_byte(int c) {
    //printf("input byte %c %i %x\n", isgraph(c)?c:'?',c,c);

    // doomkeys.h says we should uppercase the letters
    c = toupper(c);

    // for these keys we send a keypress for the special key AND the ascii
    // so they can be used to write savegame filenames among other things
    switch (c) {
        case '\n': keypress(KEY_ENTER, true); break;
        case 'Z': keypress(KEY_FIRE, true); break;
        case ' ': keypress(KEY_USE, true); break;
        case 'X': keypress(KEY_LALT, true); break;
        case '-': keypress(KEY_MINUS, true); break;
        case '+': case '=': keypress(KEY_EQUALS, true); break;
        default: break;
    }

    // send the ascii
    // TODO this doesn't seem to be working, can't press Y/N to answer question prompts
    if (c >= 0 && c <= 127) {
        keypress(c, false);
    }
}

// Whether we have an escape sequence pending. There could be a delay in the
// middle of parsing an escape sequence; we don't want to have to block while
// parsing it so we store its state here. (The only state we care about is
// whether we're parsing one.)
static bool have_csi;

// TODO we shouldn't be using getchar() below because the C file API is not
// designed to be non-blocking. We require POSIX O_NONBLOCK so we should just
// use read().

// Resumes handling of a CSI sequence.
static void handle_csi(int* c) {
    if (!have_csi)
        return;

    // ignore any count
    while (isdigit(*c))
        *c = getchar();

    // if we haven't gotten it yet, keep waiting
    if (*c == -1)
        return;
    have_csi = false;

    // convert it to a key
    int key = -1;
    switch (*c) {
        case 'A': key = KEY_UPARROW; break;
        case 'B': key = KEY_DOWNARROW; break;
        case 'C': key = KEY_RIGHTARROW; break;
        case 'D': key = KEY_LEFTARROW; break;
        default: break;
    }
    if (key != -1) {
        keypress(key, false);
    }

    *c = getchar();
}

// This function is called all over the place during rendering. We want to
// check for input often in order to get precise timing on key repeats.
void doomcli_read_input(void) {
    int c = getchar();
    for (;;) {
        if (c == -1)
            break;

        // Resume parsing any pending escape sequence
        handle_csi(&c);

        if (c != '\e') {
            handle_input_byte(c);
            c = getchar();
            continue;
        }

        // it's an escape sequence. check if the next byte is a csi ('[')
        int csi = getchar();
        if (csi == -1) {
            // give it a moment to see if the rest of an escape sequence is coming
            usleep(5000);
            csi = getchar();
        }
        if (csi != '[') {
            // not an escape sequence, just an escape char on its own
            handle_input_byte(c);
            c = csi;
            continue;
        }

        have_csi = true;
        c = getchar();
    }
}

static void simulate_release_events(void) {
    uint32_t now = DG_GetTicksMs();

    for (int key = 0; key < sizeof(keyinfos) / sizeof(*keyinfos); ++key) {
        keyinfo_t* keyinfo = &keyinfos[key];
        if (keyinfo->state == keystate_off)
            continue;

        // The amount of time in which we are expecting another key press
        uint32_t expected = 2 * key_repeat_threshold +
            ((keyinfo->state == keystate_repeat) ? key_repeat_rate : key_repeat_delay);

        // Check if enough time has passed for us to simulate the key release event
        int last_time = keyinfo->time[(keyinfo->time_next - 1 + TIME_CAPACITY) % TIME_CAPACITY];
        if (now < last_time + expected)
            continue;

        if (keyinfo->state != keystate_wait) {
            // simulate a key release event
            keybuffer[keybuffer_write].pressed = false;
            keybuffer[keybuffer_write].key = key;
            keybuffer_write = (keybuffer_write + 1) % KEYBUFFER_CAPACITY;
        }

        if (keyinfo->state == keystate_down) {
            // We've sent the key release but we're still going to wait for
            // another keypress
            keyinfo->state = keystate_wait;
        } else {
            keyinfo->state = keystate_off;
        }
    }
}

static const char* key_to_string(int key) {
    switch (key) {
        #define KEY_TO_STRING(k) case k: return #k
        KEY_TO_STRING(KEY_RIGHTARROW);
        KEY_TO_STRING(KEY_LEFTARROW);
        KEY_TO_STRING(KEY_UPARROW);
        KEY_TO_STRING(KEY_DOWNARROW);
        KEY_TO_STRING(KEY_STRAFE_L);
        KEY_TO_STRING(KEY_STRAFE_R);
        KEY_TO_STRING(KEY_USE);
        KEY_TO_STRING(KEY_FIRE);
        KEY_TO_STRING(KEY_ESCAPE);
        KEY_TO_STRING(KEY_ENTER);
        KEY_TO_STRING(KEY_TAB);
        KEY_TO_STRING(KEY_F1);
        KEY_TO_STRING(KEY_F2);
        KEY_TO_STRING(KEY_F3);
        KEY_TO_STRING(KEY_F4);
        KEY_TO_STRING(KEY_F5);
        KEY_TO_STRING(KEY_F6);
        KEY_TO_STRING(KEY_F7);
        KEY_TO_STRING(KEY_F8);
        KEY_TO_STRING(KEY_F9);
        KEY_TO_STRING(KEY_F10);
        KEY_TO_STRING(KEY_F11);
        KEY_TO_STRING(KEY_F12);
        KEY_TO_STRING(KEY_BACKSPACE);
        KEY_TO_STRING(KEY_PAUSE);
        KEY_TO_STRING(KEY_EQUALS);
        KEY_TO_STRING(KEY_MINUS);
        KEY_TO_STRING(KEY_RSHIFT);
        KEY_TO_STRING(KEY_RCTRL);
        KEY_TO_STRING(KEY_LALT);
        KEY_TO_STRING(KEY_CAPSLOCK);
        KEY_TO_STRING(KEY_NUMLOCK);
        KEY_TO_STRING(KEY_SCRLCK);
        KEY_TO_STRING(KEY_PRTSCR);
        KEY_TO_STRING(KEY_HOME);
        KEY_TO_STRING(KEY_END);
        KEY_TO_STRING(KEY_PGUP);
        KEY_TO_STRING(KEY_PGDN);
        KEY_TO_STRING(KEY_INS);
        KEY_TO_STRING(KEY_DEL);
        KEY_TO_STRING(KEYP_0);
        KEY_TO_STRING(KEYP_5);
        KEY_TO_STRING(KEYP_DIVIDE);
        KEY_TO_STRING(KEYP_PLUS);
        KEY_TO_STRING(KEYP_MULTIPLY);
        #undef KEY_TO_STRING
        default: break;
    }
    return NULL;
}

int DG_GetKey(int* out_pressed, unsigned char* out_key)
{
    doomcli_read_input();
    simulate_release_events();

    if (keybuffer_read != keybuffer_write) {
        *out_pressed = keybuffer[keybuffer_read].pressed;
        *out_key = keybuffer[keybuffer_read].key;
        keybuffer_read = (keybuffer_read + 1) % KEYBUFFER_CAPACITY;

        #if 0
        fputs("key ", stdout);
        const char* str = key_to_string(*out_key);
        if (str)
            fputs(str, stdout);
        else if (isgraph(*out_key))
            putchar(*out_key);
        else
            fputs("<unknown>", stdout);
        printf(" %s\n", *out_pressed ? "pressed" : "released");
        #endif

        return 1;
    }
    return 0;
}

void DG_SetWindowTitle(const char * title)
{
    // TODO look out for special characters in title (unlikely with doom but
    // maybe third party wad files would have them)
    // https://unix.stackexchange.com/questions/618837/set-window-title-to-arbitrary-sequence-of-characters-in-the-st-terminal-emulator
    printf("\033]0;%s\007", title);
}

static void show_cursor(void) {
    puts("\033[?25h");
}

int main(int argc, char **argv)
{
    atexit(show_cursor);

//for (int i = 0; i < 64; ++i){printf("%u ",i);puts(sextants[i]);}abort();
    doomgeneric_Create(argc, argv);
    for (;;) {
        doomgeneric_Tick();
    }
    return 0;
}
