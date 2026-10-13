#pragma once
#include <stddef.h>
#include <stdint.h>
#ifndef NULL
#define NULL 0
#endif

void *memset(void *s, int c, size_t len);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int   memcmp(const void *s1, const void *s2, size_t n);

#define UART ((volatile unsigned int *)0x11002000)
void putc(char c) {
  if (c == '\n')
    putc('\r');
  while(!(UART[5] & 0x20));
  UART[0] = (unsigned)c;
}

void puts(const char *s) {
  while (*s) {
    putc(*s++);
  }
}

int getchar() {
    if (UART[5] & 0x01) {
      return UART[0];
    } else {
      return -1;
    }
}

#define APXGPT4 ((volatile uint32_t *)0x10008048)
int usleep(uint32_t s) {
  uint32_t ticks = s * 13;
  uint32_t start = *APXGPT4;

  while ((*APXGPT4 - start) < ticks)
    ;
  return 0;
}

static inline uint32_t get_ticks(void) {
    return *APXGPT4;
}

#ifdef __riscv
void *memset(void *s, int c, size_t len) {
  unsigned char *p = s;
  while (len--) {
    *p++ = (unsigned char)c;
  }
  return s;
}

static uint32_t __udivmod32(uint32_t num, uint32_t den, uint32_t *rem) {
  if (den == 0) {
    if (rem)
      *rem = 0;
    return 0;
  }

  uint32_t q = 0;
  uint32_t r = 0;
  int i;
  for (i = 31; i >= 0; i--) {
    r <<= 1;
    r |= (num >> i) & 1U;
    if (r >= den) {
      r -= den;
      q |= (1U << i);
    }
  }

  if (rem)
    *rem = r;
  return q;
}

int32_t __divsi3(int32_t a, int32_t b) {
  int neg = (a < 0) ^ (b < 0);
  uint32_t ua = (a < 0) ? (uint32_t)-a : (uint32_t)a;
  uint32_t ub = (b < 0) ? (uint32_t)-b : (uint32_t)b;
  uint32_t q = __udivmod32(ua, ub, NULL);
  return neg ? -(int32_t)q : (int32_t)q;
}

int32_t __modsi3(int32_t a, int32_t b) {
  int neg = (a < 0);
  uint32_t ua = (a < 0) ? (uint32_t)-a : (uint32_t)a;
  uint32_t ub = (b < 0) ? (uint32_t)-b : (uint32_t)b;
  uint32_t r;
  __udivmod32(ua, ub, &r);
  return neg ? -(int32_t)r : (int32_t)r;
}

uint32_t __udivsi3(uint32_t a, uint32_t b) { return __udivmod32(a, b, NULL); }

uint32_t __umodsi3(uint32_t a, uint32_t b) {
  uint32_t r;
  __udivmod32(a, b, &r);
  return r;
}

#endif

int32_t __mulsi3(int32_t a, int32_t b) {
  int32_t res = 0;
  uint32_t ua = (uint32_t)a;
  uint32_t ub = (uint32_t)b;
  while (ub > 0) {
    if (ub & 1U)
      res += (int32_t)ua;
    ua <<= 1;
    ub >>= 1;
  }
  return res;
}

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"
#define printf(...) npf_pprintf((void (*)(int,  void *))&putc, NULL, __VA_ARGS__)

unsigned int getrandseed() {
  return *APXGPT4;
}

#define RAND_MAX 0x7FFFFFFF

static unsigned int state = 1;

void srand(unsigned int seed) {
    state = (seed != 0) ? seed : 1;
}

int rand(void) {
    unsigned int x = state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    state = x;
    
    return (int)(x & RAND_MAX);
}

void *malloc(size_t size) {
    static uint8_t heap[2048];
    static size_t heap_idx = 0;
    // Align to 4 bytes
    heap_idx = (heap_idx + 3) & ~3;
    if (heap_idx + size > sizeof(heap)) return NULL;
    void *ptr = &heap[heap_idx];
    heap_idx += size;
    return ptr;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

#ifdef __riscv
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}
#endif

size_t strlen(const char *s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

#define TICKS_PER_SEC 13000000UL
long time(long *t) {
    uint32_t ticks = get_ticks();
    long sec = (long)(ticks / TICKS_PER_SEC);
    if (t) *t = sec;
    return sec;
}

void exit(int status) {
    printf("\n[Process exited with status %d]\n", status);
    for(;;);
}
