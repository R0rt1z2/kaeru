#pragma once

/** \name Fixed width integers
 *  @{
 */
typedef unsigned char u8_t;             ///< Unsigned 8-bit type
typedef unsigned short int u16_t;       ///< Unsigned 16-bit type
typedef unsigned int u32_t;             ///< Unsigned 32-bit type
typedef unsigned long long u64_t;        ///< Unsigned 64-bit type

typedef u64_t u64;
typedef u32_t u32;
typedef u16_t u16;
typedef u8_t u8;

typedef unsigned size_t;

size_t strlen(const char *str);
int printf(const char *format, ...);
int sprintf (char *str, const char *format, ...);
char *strcpy(char *to, const char *from);
int strncmp(const char *s1, const char *s2, u32_t n);
void*  memset(void*  dst, int c, u32_t n);
void *memcpy(void *dest, const void *src, size_t n);
int strcmp(const char *s1, const char *s2);

int puts(const char *line);
int memcmp(const void* s1, const void* s2,size_t n);
int utf16le_strcmp(const u8_t *utf16le_str, const char *ascii_str);