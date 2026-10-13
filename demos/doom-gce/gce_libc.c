#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_ALT_FORM_FLAG 1
#define NANOPRINTF_USE_FLOAT_SINGLE_PRECISION 0
#define NANOPRINTF_USE_FLOAT_HEX_FORMAT_SPECIFIER 0
#define NANOPRINTF_USE_FLOAT_SCI_FORMAT_SPECIFIER 1
#define NANOPRINTF_USE_FLOAT_SHORTEST_FORMAT_SPECIFIER 1
#define NANOPRINTF_IMPLEMENTATION
#include "../nanoprintf.h"

#define UART_BASE ((volatile unsigned int *)0x11002000u)
#define UART_STATUS ((volatile unsigned int *)0x11002014u)
#define GPT4_BASE ((volatile unsigned int *)0x10008048u)
#define TICKS_PER_SEC 13000000UL

int errno = 0;

static void gce_uart_raw(char c) {
    while (!(*UART_STATUS & 0x20u)) {}
    *UART_BASE = (unsigned int)(unsigned char)c;
}

static void gce_uart_putc(int c, void *ctx) {
    (void)ctx;
    char ch = (char)c;
    if (ch == '\n') {
        gce_uart_raw('\r');
        gce_uart_raw('\n');
    } else {
        gce_uart_raw(ch);
    }
}

static inline uint32_t gce_get_ticks(void) {
    return *GPT4_BASE;
}

struct _GCE_FILE {
    int is_valid;
    int is_embedded;
    int is_write;
    int is_stdout;
    const unsigned char *ro_data;
    unsigned int ro_len;
    unsigned int pos;
    int eof;
    int err;
};

static FILE __gce_stdin_file;
static FILE __gce_stdout_file;
static FILE __gce_stderr_file;
FILE *stdin = &__gce_stdin_file;
FILE *stdout = &__gce_stdout_file;
FILE *stderr = &__gce_stderr_file;

#define GCE_HEAP_SIZE (8u * 1024u * 1024u)
static unsigned char gce_heap[GCE_HEAP_SIZE] __attribute__((aligned(8)));

typedef struct heap_block {
    unsigned int size;
    struct heap_block *next;
    int free;
} heap_block_t;

static heap_block_t *heap_head = 0;
static int heap_init_done = 0;

static void heap_init(void) {
    if (heap_init_done) return;
    heap_head = (heap_block_t *)gce_heap;
    heap_head->size = GCE_HEAP_SIZE - sizeof(heap_block_t);
    heap_head->next = 0;
    heap_head->free = 1;
    heap_init_done = 1;
    __gce_stdout_file.is_valid = 1;
    __gce_stdout_file.is_stdout = 1;
    __gce_stdout_file.is_write = 1;
    __gce_stderr_file.is_valid = 1;
    __gce_stderr_file.is_stdout = 1;
    __gce_stderr_file.is_write = 1;
    __gce_stdin_file.is_valid = 1;
}

void *malloc(size_t size) {
    heap_init();
    if (size == 0) size = 1;
    size = (size + 7u) & ~7u;
    heap_block_t *b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            if (b->size >= size + sizeof(heap_block_t) + 8) {
                heap_block_t *nb = (heap_block_t *)((unsigned char *)b + sizeof(heap_block_t) + size);
                nb->size = b->size - size - sizeof(heap_block_t);
                nb->next = b->next;
                nb->free = 1;
                b->size = size;
                b->next = nb;
            }
            b->free = 0;
            return (void *)((unsigned char *)b + sizeof(heap_block_t));
        }
        b = b->next;
    }
    return 0;
}

void free(void *ptr) {
    if (!ptr) return;
    heap_block_t *b = (heap_block_t *)((unsigned char *)ptr - sizeof(heap_block_t));
    b->free = 1;
    while (b->next && b->next->free) {
        b->size += sizeof(heap_block_t) + b->next->size;
        b->next = b->next->next;
    }
}

void *calloc(size_t nmemb, size_t size) {
    unsigned long total = (unsigned long)nmemb * (unsigned long)size;
    if (nmemb && total / nmemb != (unsigned long)size) return 0;
    void *p = malloc(total);
    if (!p) return 0;
    unsigned char *d = (unsigned char *)p;
    for (unsigned long i = 0; i < total; i++) d[i] = 0;
    return p;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }
    heap_block_t *b = (heap_block_t *)((unsigned char *)ptr - sizeof(heap_block_t));
    unsigned int old = b->size;
    size = (size + 7u) & ~7u;
    if (old >= size) {
        if (old >= size + sizeof(heap_block_t) + 8) {
            heap_block_t *nb = (heap_block_t *)((unsigned char *)b + sizeof(heap_block_t) + size);
            nb->size = old - size - sizeof(heap_block_t);
            nb->next = b->next;
            nb->free = 1;
            b->size = size;
            b->next = nb;
        }
        return ptr;
    }
    if (b->next && b->next->free &&
        old + sizeof(heap_block_t) + b->next->size >= size) {
        unsigned int need = size - old;
        if (b->next->size > need) {
            heap_block_t *nn = (heap_block_t *)((unsigned char *)b->next + need);
            unsigned int next_old = b->next->size;
            nn->size = next_old - need;
            nn->next = b->next->next;
            nn->free = 1;
            b->size = size;
            b->next = nn;
        } else {
            b->size = old + sizeof(heap_block_t) + b->next->size;
            b->next = b->next->next;
        }
        return ptr;
    }
    void *np = malloc(size);
    if (!np) return 0;
    unsigned char *d = (unsigned char *)np;
    unsigned char *s = (unsigned char *)ptr;
    for (unsigned int i = 0; i < old; i++) d[i] = s[i];
    free(ptr);
    return np;
}

/* ---- ctype ---- */
int isalpha(int c) { return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')); }
int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
int islower(int c) { return (c >= 'a' && c <= 'z'); }
int isdigit(int c) { return (c >= '0' && c <= '9'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isspace(int c) { return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'); }
int isprint(int c) { return (c >= 32 && c < 127); }
int isgraph(int c) { return (c > 32 && c < 127); }
int iscntrl(int c) { return (c < 32 || c == 127); }
int ispunct(int c) { return isprint(c) && !isalnum(c) && c != ' '; }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}
char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dest[i] = src[i];
    for (; i < n; i++) dest[i] = '\0';
    return dest;
}
char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++));
    return dest;
}
char *strncat(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (*d) d++;
    size_t i = 0;
    for (; i < n && src[i]; i++) d[i] = src[i];
    d[i] = '\0';
    return dest;
}
int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2) { s1++; s2++; }
    return (unsigned char)*s1 - (unsigned char)*s2;
}
int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char a = (unsigned char)s1[i], b = (unsigned char)s2[i];
        if (a != b) return a - b;
        if (a == 0) return 0;
    }
    return 0;
}
char *strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s) { if (*s == ch) return (char *)s; s++; }
    return (ch == 0) ? (char *)s : 0;
}
char *strrchr(const char *s, int c) {
    char ch = (char)c;
    const char *last = 0;
    while (*s) { if (*s == ch) last = s; s++; }
    if (ch == 0) return (char *)s;
    return (char *)last;
}
char *strdup(const char *s) {
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (!d) return 0;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return d;
}
char *strstr(const char *h, const char *n) {
    if (!*n) return (char *)h;
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return (char *)h;
    }
    return 0;
}
size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (s[n]) {
        const char *a = accept;
        int found = 0;
        while (*a) { if (*a == s[n]) { found = 1; break; } a++; }
        if (!found) break;
        n++;
    }
    return n;
}
size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (s[n]) {
        const char *a = reject;
        int found = 0;
        while (*a) { if (*a == s[n]) { found = 1; break; } a++; }
        if (found) break;
        n++;
    }
    return n;
}
void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = (const unsigned char *)s;
    for (size_t i = 0; i < n; i++) if (p[i] == (unsigned char)c) return (void *)(p + i);
    return 0;
}
size_t strnlen(const char *s, size_t maxlen) {
    size_t n = 0;
    while (n < maxlen && s[n]) n++;
    return n;
}
static char *strtok_save = 0;
char *strtok(char *str, const char *delim) {
    char *s = str ? str : strtok_save;
    if (!s) return 0;
    /* skip leading delims */
    while (*s) {
        const char *d = delim;
        int isdel = 0;
        while (*d) { if (*d == *s) { isdel = 1; break; } d++; }
        if (!isdel) break;
        s++;
    }
    if (!*s) { strtok_save = 0; return 0; }
    char *start = s;
    while (*s) {
        const char *d = delim;
        int isdel = 0;
        while (*d) { if (*d == *s) { isdel = 1; break; } d++; }
        if (isdel) { *s = '\0'; strtok_save = s + 1; return start; }
        s++;
    }
    strtok_save = 0;
    return start;
}
char *strerror(int errnum) {
    (void)errnum;
    return "error";
}
int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int a = tolower((unsigned char)*s1), b = tolower((unsigned char)*s2);
        if (a != b) return a - b;
        s1++; s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}
int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char a = (unsigned char)s1[i], b = (unsigned char)s2[i];
        int la = tolower(a), lb = tolower(b);
        if (la != lb) return la - lb;
        if (a == 0) return 0;
    }
    return 0;
}

/* ---- stdlib ---- */
int atoi(const char *s) {
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    int v = 0;
    while (isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}
long atol(const char *s) { return (long)atoi(s); }

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
long strtol(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') base = 8;
        else base = 10;
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    long v = 0;
    while (*s) {
        int d = hexval(*s);
        if (d < 0 || d >= base) break;
        v = v * base + d;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -v : v;
}
unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s = nptr;
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    if (base == 0) {
        if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { base = 16; s += 2; }
        else if (s[0] == '0') base = 8;
        else base = 10;
    } else if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
    }
    unsigned long v = 0;
    while (*s) {
        int d = hexval(*s);
        if (d < 0 || d >= base) break;
        v = v * base + (unsigned long)d;
        s++;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? (unsigned long)(-(long)v) : v;
}
double strtod(const char *nptr, char **endptr) {
    const char *s = nptr;
    while (isspace((unsigned char)*s)) s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    double v = 0.0;
    while (isdigit((unsigned char)*s)) { v = v * 10.0 + (*s - '0'); s++; }
    if (*s == '.') {
        s++;
        double frac = 0.0, base = 0.1;
        while (isdigit((unsigned char)*s)) { frac += (*s - '0') * base; base *= 0.1; s++; }
        v += frac;
    }
    if (*s == 'e' || *s == 'E') {
        s++;
        int eneg = 0;
        if (*s == '-') { eneg = 1; s++; }
        else if (*s == '+') s++;
        int e = 0;
        while (isdigit((unsigned char)*s)) { e = e * 10 + (*s - '0'); s++; }
        double mul = 1.0;
        for (int i = 0; i < e; i++) mul *= 10.0;
        v = eneg ? v / mul : v * mul;
    }
    if (endptr) *endptr = (char *)s;
    return neg ? -v : v;
}
double atof(const char *s) { return strtod(s, 0); }

static unsigned int rand_state = 1;
void srand(unsigned int seed) { rand_state = seed ? seed : 1; }
int rand(void) {
    unsigned int x = rand_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rand_state = x;
    return (int)(x & RAND_MAX);
}
int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
void *mempcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return d + n;
}
char *stpcpy(char *dest, const char *src) {
    while ((*dest++ = *src++));
    return dest - 1;
}
char *getenv(const char *name) { (void)name; return 0; }
int system(const char *s) { (void)s; return -1; }

static void (*atexit_funcs[8])(void);
static int atexit_count = 0;
int atexit(void (*func)(void)) {
    if (atexit_count >= 8) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

void __gce_assert_fail(const char *expr, const char *file, int line) {
    npf_pprintf(gce_uart_putc, 0, "assert failed: %s:%d: %s\n", file, line, expr);
    /* park */
    for (;;) {}
}

void abort(void) {
    npf_pprintf(gce_uart_putc, 0, "abort\n");
    for (;;) {}
}
void exit(int status) {
    for (int i = atexit_count - 1; i >= 0; i--) atexit_funcs[i]();
    npf_pprintf(gce_uart_putc, 0, "\n[exit %d]\n", status);
    for (;;) {}
}

/* ---- time ---- */
time_t time(time_t *t) {
    uint32_t ticks = gce_get_ticks();
    long sec = (long)(ticks / TICKS_PER_SEC);
    if (t) *t = sec;
    return sec;
}
clock_t clock(void) {
    return (clock_t)gce_get_ticks();
}
int gettimeofday(struct timeval *tp, struct timezone *tzp) {
    (void)tzp;
    uint32_t ticks = gce_get_ticks();
    tp->tv_sec = (long)(ticks / TICKS_PER_SEC);
    tp->tv_usec = (long)((ticks % TICKS_PER_SEC) * 1000000UL / TICKS_PER_SEC);
    return 0;
}
int usleep(unsigned int usec) {
    uint32_t start = gce_get_ticks();
    uint32_t delta = usec * 13u;
    while ((gce_get_ticks() - start) < delta) {}
    return 0;
}
unsigned int sleep(unsigned int seconds) {
    usleep(seconds * 1000000u);
    return 0;
}

ssize_t read(int fd, void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}
ssize_t write(int fd, const void *buf, size_t count) {
    if (fd == 1 || fd == 2) {
        const char *p = (const char *)buf;
        for (size_t i = 0; i < count; i++) gce_uart_putc(p[i], 0);
        return (ssize_t)count;
    }
    return (ssize_t)count;
}
int close(int fd) { (void)fd; return 0; }
int isatty(int fd) { (void)fd; return 0; }
int fileno(void *stream) {
    if (stream == (void *)stdout) return 1;
    if (stream == (void *)stderr) return 2;
    return 0;
}
int open(const char *path, int flags, ...) { (void)path; (void)flags; return -1; }
int fcntl(int fd, int cmd, ...) { (void)fd; (void)cmd; return 0; }
int tcgetattr(int fd, struct termios *t) { (void)fd; if (t) t->c_lflag = 0; return 0; }
int tcsetattr(int fd, int act, const struct termios *t) { (void)fd; (void)act; (void)t; return 0; }
void cfmakeraw(struct termios *t) { (void)t; }
int stat(const char *path, struct stat *buf) { (void)path; if (buf) buf->st_size = 0; return -1; }
int mkdir(const char *path, unsigned int mode) { (void)path; (void)mode; return 0; }
int setvbuf(void *stream, char *buf, int mode, size_t size) {
    (void)stream; (void)buf; (void)mode; (void)size; return 0;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = npf_vpprintf(gce_uart_putc, 0, fmt, ap);
    va_end(ap);
    return r;
}
int vprintf(const char *fmt, va_list ap) {
    return npf_vpprintf(gce_uart_putc, 0, fmt, ap);
}
int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    if (stream == stdout || stream == stderr || stream == 0) {
        return npf_vpprintf(gce_uart_putc, 0, fmt, ap);
    }
    char tmp[256];
    int n = npf_vsnprintf(tmp, sizeof(tmp), fmt, ap);
    if (stream && stream->is_write) {
        stream->pos += (unsigned int)(n < 0 ? 0 : n);
    }
    return n;
}
int fprintf(FILE *stream, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vfprintf(stream, fmt, ap);
    va_end(ap);
    return r;
}
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    return npf_vsnprintf(str, size, fmt, ap);
}
int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = npf_vsnprintf(str, size, fmt, ap);
    va_end(ap);
    return r;
}
int sprintf(char *str, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = npf_vsnprintf(str, (size_t)0x7fffffff, fmt, ap);
    va_end(ap);
    return r;
}
int puts(const char *s) {
    while (*s) gce_uart_putc(*s++, 0);
    gce_uart_putc('\n', 0);
    return 0;
}
int putchar(int c) { gce_uart_putc(c, 0); return c; }
int getchar(void) {
    if (*UART_STATUS & 0x01u) {
        return (int)(*UART_BASE & 0xFFu);
    }
    return -1;
}
int fputs(const char *s, FILE *stream) {
    if (stream == stdout || stream == stderr || stream == 0) {
        while (*s) gce_uart_putc(*s++, 0);
        return 0;
    }
    if (stream && stream->is_write) {
        while (*s) { stream->pos++; s++; }
        return 0;
    }
    return -1;
}
int fputc(int c, FILE *stream) {
    if (stream == stdout || stream == stderr || stream == 0) {
        gce_uart_putc(c, 0);
        return c;
    }
    if (stream && stream->is_write) { stream->pos++; return c; }
    return -1;
}
int fgetc(FILE *stream) {
    if (!stream || !stream->is_valid) return -1;
    if (stream == stdin || stream == &__gce_stdin_file) return getchar();
    if (stream->is_embedded) {
        if (stream->pos >= stream->ro_len) { stream->eof = 1; return -1; }
        return stream->ro_data[stream->pos++];
    }
    return -1;
}
char *fgets(char *s, int size, FILE *stream) {
    if (!stream || !stream->is_valid || size <= 1) return 0;
    int i = 0;
    while (i < size - 1) {
        int c = fgetc(stream);
        if (c < 0) break;
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    if (i == 0) return 0;
    s[i] = '\0';
    return s;
}
int sscanf(const char *str, const char *fmt, ...) {
    (void)str; (void)fmt;
    return -1;
}
int fscanf(FILE *stream, const char *fmt, ...) {
    (void)stream; (void)fmt;
    return -1;
}

extern const unsigned char doom_wad[];
extern const unsigned int doom_wad_len;

#define GCE_MAX_OPEN 8
static FILE gce_files[GCE_MAX_OPEN];

static int str_ends_with_wad(const char *p) {
    size_t n = strlen(p);
    if (n < 4) return 0;
    const char *e = p + n - 4;
    return (e[0] == '.' || e[0] == '.') &&
           (tolower((unsigned char)e[1]) == 'w') &&
           (tolower((unsigned char)e[2]) == 'a') &&
           (tolower((unsigned char)e[3]) == 'd');
}
static int str_contains_doom1(const char *p) {
    for (; *p; p++) {
        if (tolower((unsigned char)p[0]) == 'd' &&
            tolower((unsigned char)p[1]) == 'o' &&
            tolower((unsigned char)p[2]) == 'o' &&
            tolower((unsigned char)p[3]) == 'm' &&
            p[4] == '1') return 1;
    }
    return 0;
}

FILE *fopen(const char *path, const char *mode) {
    heap_init();
    if (!path || !mode) { errno = 22; return 0; }
    int want_write = 0;
    for (const char *m = mode; *m; m++) {
        if (*m == 'w' || *m == 'a' || *m == '+') { want_write = 1; break; }
    }
    int is_read = !want_write;
    if (is_read) {
        if (!str_ends_with_wad(path) || !str_contains_doom1(path)) {
            errno = 2;
            return 0;
        }
        for (int i = 0; i < GCE_MAX_OPEN; i++) {
            if (!gce_files[i].is_valid) {
                gce_files[i].is_valid = 1;
                gce_files[i].is_embedded = 1;
                gce_files[i].is_write = 0;
                gce_files[i].is_stdout = 0;
                gce_files[i].ro_data = doom_wad;
                gce_files[i].ro_len = doom_wad_len;
                gce_files[i].pos = 0;
                gce_files[i].eof = 0;
                gce_files[i].err = 0;
                return &gce_files[i];
            }
        }
        errno = 12;
        return 0;
    } else {
        for (int i = 0; i < GCE_MAX_OPEN; i++) {
            if (!gce_files[i].is_valid) {
                gce_files[i].is_valid = 1;
                gce_files[i].is_embedded = 0;
                gce_files[i].is_write = 1;
                gce_files[i].is_stdout = 0;
                gce_files[i].ro_data = 0;
                gce_files[i].ro_len = 0;
                gce_files[i].pos = 0;
                gce_files[i].eof = 0;
                gce_files[i].err = 0;
                return &gce_files[i];
            }
        }
        errno = 12;
        return 0;
    }
}
int fclose(FILE *stream) {
    if (!stream) return -1;
    if (stream == stdout || stream == stderr || stream == stdin) return 0;
    stream->is_valid = 0;
    stream->is_embedded = 0;
    stream->is_write = 0;
    return 0;
}
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !stream->is_valid || size == 0 || nmemb == 0) return 0;
    if (stream->is_write) return 0;
    if (!stream->is_embedded) return 0;
    unsigned long total = (unsigned long)size * (unsigned long)nmemb;
    unsigned int avail = stream->ro_len > stream->pos ? stream->ro_len - stream->pos : 0;
    if (total > avail) {
        total = avail;
        stream->eof = 1;
    }
    unsigned char *d = (unsigned char *)ptr;
    const unsigned char *s = stream->ro_data + stream->pos;
    for (unsigned long i = 0; i < total; i++) d[i] = s[i];
    stream->pos += (unsigned int)total;
    return total / size;
}
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr;
    if (!stream || !stream->is_valid) return 0;
    if (stream == stdout || stream == stderr) {
        const char *p = (const char *)ptr;
        unsigned long total = (unsigned long)size * (unsigned long)nmemb;
        for (unsigned long i = 0; i < total; i++) gce_uart_putc(p[i], 0);
        return nmemb;
    }
    if (!stream->is_write) { stream->err = 1; return 0; }
    stream->pos += (unsigned int)((unsigned long)size * (unsigned long)nmemb);
    return nmemb;
}
int fseek(FILE *stream, long offset, int whence) {
    if (!stream || !stream->is_valid) return -1;
    long newpos;
    if (whence == 0) newpos = offset;
    else if (whence == 1) newpos = (long)stream->pos + offset;
    else if (whence == 2) {
        if (stream->is_embedded) newpos = (long)stream->ro_len + offset;
        else newpos = (long)stream->pos + offset;
    } else return -1;
    if (newpos < 0) return -1;
    if (stream->is_embedded && (unsigned long)newpos > stream->ro_len) return -1;
    stream->pos = (unsigned int)newpos;
    stream->eof = 0;
    return 0;
}
long ftell(FILE *stream) {
    if (!stream || !stream->is_valid) return -1;
    return (long)stream->pos;
}
void rewind(FILE *stream) {
    if (!stream || !stream->is_valid) return;
    stream->pos = 0;
    stream->eof = 0;
    stream->err = 0;
}
int feof(FILE *stream) {
    if (!stream) return 1;
    return stream->eof;
}
int ferror(FILE *stream) {
    if (!stream) return 1;
    return stream->err;
}
int fflush(FILE *stream) { (void)stream; return 0; }
int remove(const char *path) { (void)path; return 0; }
int rename(const char *oldpath, const char *newpath) { (void)oldpath; (void)newpath; return 0; }
