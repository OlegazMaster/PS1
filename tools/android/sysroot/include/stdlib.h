#ifndef _STDLIB_H
#define _STDLIB_H
#include <stddef.h>
void *malloc(size_t n);
void *calloc(size_t n, size_t size);
void *realloc(void *p, size_t n);
void free(void *p);
void exit(int status) __attribute__((noreturn));
void abort(void) __attribute__((noreturn));
int atoi(const char *s);
unsigned long strtoul(const char *s, char **end, int base);
char *getenv(const char *name);
#endif
