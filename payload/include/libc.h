#pragma once

typedef unsigned char u8_t;
typedef unsigned short int u16_t;
typedef unsigned int u32_t;
typedef unsigned long long u64_t;
typedef unsigned size_t;

size_t strlen(const char *str);
char *strcpy(char *to, const char *from);
char *strcat(char *dest, const char *src);
size_t strspn(const char *str, const char *accept);
size_t strcspn(const char *str, const char *reject);
const char* strchr(const char* str, int c);
char* strdup(const char* s);
char* strtok(char* str, const char* delim);
long strtol(const char* str, char** endptr, int base);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, u32_t n);
int utf16le_strcmp(const u8_t *utf16le_str, const char *ascii_str);
void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memset(void *dst, int c, u32_t n);
int snprintf(char *buf, size_t size, const char *fmt, ...);
void hexdump(const void *data, size_t len);
