#pragma once
#include <stddef.h>
#include <stdarg.h>

#ifndef NULL
#define NULL 0
#endif

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

#define BUFSIZ 512
#define EOF (-1)

#define _IONBF 2
#define _IOLBF 1
#define _IOFBF 0

typedef struct _GCE_FILE FILE;

extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

int setvbuf(void *stream, char *buf, int mode, size_t size);

int printf(const char *format, ...);
int fprintf(FILE *stream, const char *format, ...);
int sprintf(char *str, const char *format, ...);
int snprintf(char *str, size_t size, const char *format, ...);
int vsnprintf(char *str, size_t size, const char *format, va_list ap);
int vfprintf(FILE *stream, const char *format, va_list ap);
int vprintf(const char *format, va_list ap);

FILE *fopen(const char *path, const char *mode);
int fclose(FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
void rewind(FILE *stream);
int feof(FILE *stream);
int ferror(FILE *stream);
int fflush(FILE *stream);
char *fgets(char *s, int size, FILE *stream);
int fputs(const char *s, FILE *stream);
int fputc(int c, FILE *stream);
int fgetc(FILE *stream);
int sscanf(const char *str, const char *format, ...);
int fscanf(FILE *stream, const char *format, ...);

int puts(const char *s);
int putchar(int c);
int getchar(void);
int remove(const char *path);
int rename(const char *oldpath, const char *newpath);
