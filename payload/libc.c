#include "include/libc.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

u32_t strlen(const char *str) {
    const char *s;

    for (s = str; *s; ++s)
        ;
    return (s - str);
}

char *strcpy(char *to, const char *from) {
    char *save = to;
    for (; (*to = *from) != '\0'; ++from, ++to)
        ;
    return (save);
}

char *strcat(char *dest, const char *src) {
    strcpy(dest + strlen(dest), src);
    return dest;
}

size_t strspn(const char *str, const char *accept) {
    const char *p;
    const char *a;
    size_t count = 0;

    for (p = str; *p != '\0'; ++p) {
        for (a = accept; *a != '\0'; ++a) {
            if (*p == *a) {
                break;
            }
        }
        if (*a == '\0') {
            return count;
        } else {
            ++count;
        }
    }
    return count;
}

size_t strcspn(const char *str, const char *reject) {
    const char *p;
    const char *r;
    size_t count = 0;

    for (p = str; *p != '\0'; ++p) {
        for (r = reject; *r != '\0'; ++r) {
            if (*p == *r) {
                return count;
            }
        }
        ++count;
    }
    return count;
}

const char* strchr(const char* str, int c) {
    while (*str != '\0') {
        if (*str == (char)c) {
            return str;
        }
        str++;
    }
    return NULL;
}

char* strdup(const char* s) {
    static char buffer[256];
    size_t len = strlen(s);

    if (len >= 256) {
        return NULL;
    }

    memcpy(buffer, s, len + 1);
    return buffer;
}

char* strtok(char* str, const char* delim) {
    static char* last_token = NULL;
    if (str == NULL) {
        str = last_token;
    }
    if (str == NULL) return NULL;

    str += strspn(str, delim);
    if (*str == '\0') {
        last_token = NULL;
        return NULL;
    }

    char* end = str + strcspn(str, delim);
    if (*end == '\0') {
        last_token = NULL;
    } else {
        *end = '\0';
        last_token = end + 1;
    }
    return str;
}

long strtol(const char* str, char** endptr, int base) {
    const char* p = str;
    long value = 0;
    int sign = 1;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v' || *p == '\f' || *p == '\r') {
        ++p;
    }

    if (*p == '+' || *p == '-') {
        sign = (*p++ == '-') ? -1 : 1;
    }

    if ((base == 0 || base == 16) &&
        *p == '0' && (*(p + 1) == 'x' || *(p + 1) == 'X')) {
        p += 2;
        base = 16;
    }

    if (base == 0) {
        base = (*p == '0') ? 8 : 10;
    }

    while (1) {
        int digit;

        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (*p >= 'a' && *p <= 'z') digit = *p - 'a' + 10;
        else if (*p >= 'A' && *p <= 'Z') digit = *p - 'A' + 10;
        else break;

        if (digit >= base) break;

        value = value * base + digit;
        ++p;
    }

    if (endptr) *endptr = (char*)p;

    return value * sign;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 == *s2++)
        if (*s1++ == 0)
            return (0);
    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

int strncmp(const char *s1, const char *s2, u32_t n) {
    if (n == 0)
        return (0);
    do {
        if (*s1 != *s2++)
            return (*(unsigned char *)s1 - *(unsigned char *)--s2);
        if (*s1++ == 0)
            break;
    } while (--n != 0);
    return (0);
}

int utf16le_strcmp(const u8_t *utf16le_str, const char *ascii_str) {
    while (*ascii_str) {
        if (*utf16le_str != (u8_t)(*ascii_str))
            return -1;

        utf16le_str += 2;
        ascii_str++;
    }

    if (*utf16le_str != 0x00 || *(utf16le_str + 1) != 0x00)
        return -1;

    return 0;
}

void *memcpy(void *dest, const void *src, size_t n) {
    char *dp = dest;
    const char *sp = src;
    while (n--)
        *dp++ = *sp++;
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = s1, *p2 = s2;
    while (n--)
        if (*p1 != *p2)
            return *p1 - *p2;
        else
            p1++, p2++;
    return 0;
}

void *memset(void *dst, int c, u32_t n) {
    char *q = dst;
    char *end = q + n;

    for (;;) {
        if (q >= end)
            break;
        *q++ = (char)c;
        if (q >= end)
            break;
        *q++ = (char)c;
        if (q >= end)
            break;
        *q++ = (char)c;
        if (q >= end)
            break;
        *q++ = (char)c;
    }

    return dst;
}

struct printf_info {
    char *bf;     /* Digit buffer */
    char zs;      /* non-zero if a digit has been written */
    char *outstr; /* Next output position for sprintf() */

    /* Output a character */
    void (*putc)(struct printf_info *info, char ch);
};

static void putc_outstr(struct printf_info *info, char ch) {
    *info->outstr++ = ch;
}

static void out(struct printf_info *info, char c) { *info->bf++ = c; }

static void out_dgt(struct printf_info *info, char dgt) {
    out(info, dgt + (dgt < 10 ? '0' : 'a' - 10));
    info->zs = 1;
}

static void div_out(struct printf_info *info, unsigned long *num,
                    unsigned long div) {
    unsigned char dgt = 0;

    while (*num >= div) {
        *num -= div;
        dgt++;
    }

    if (info->zs || dgt > 0)
        out_dgt(info, dgt);
}

static int _vprintf(struct printf_info *info, const char *fmt, va_list va) {
    char ch;
    char *p;
    unsigned long num;
    char buf[12];
    unsigned long div;

    while ((ch = *(fmt++))) {
        if (ch != '%') {
            info->putc(info, ch);
        } else {
            bool lz = false;
            int width = 0;
            bool islong = false;

            ch = *(fmt++);
            if (ch == '-')
                ch = *(fmt++);

            if (ch == '0') {
                ch = *(fmt++);
                lz = 1;
            }

            if (ch >= '0' && ch <= '9') {
                width = 0;
                while (ch >= '0' && ch <= '9') {
                    width = (width * 10) + ch - '0';
                    ch = *fmt++;
                }
            }
            if (ch == 'l') {
                ch = *(fmt++);
                islong = true;
            }

            info->bf = buf;
            p = info->bf;
            info->zs = 0;

            switch (ch) {
                case '\0':
                    goto abort;
                case 'u':
                case 'd':
                case 'i':
                    div = 1000000000;
                    if (islong) {
                        num = va_arg(va, unsigned long);
                        if (sizeof(long) > 4)
                            div *= div * 10;
                    } else {
                        num = va_arg(va, unsigned int);
                    }

                    if (ch != 'u') {
                        if (islong && (long)num < 0) {
                            num = -(long)num;
                            out(info, '-');
                        } else if (!islong && (int)num < 0) {
                            num = -(int)num;
                            out(info, '-');
                        }
                    }
                    if (!num) {
                        out_dgt(info, 0);
                    } else {
                        for (; div; div /= 10)
                            div_out(info, &num, div);
                    }
                    break;
                case 'p':
                    islong = true;
                    /* no break */
                case 'x':
                    if (islong) {
                        num = va_arg(va, unsigned long);
                        div = 1UL << (sizeof(long) * 8 - 4);
                    } else {
                        num = va_arg(va, unsigned int);
                        div = 0x10000000;
                    }
                    if (!num) {
                        out_dgt(info, 0);
                    } else {
                        for (; div; div /= 0x10)
                            div_out(info, &num, div);
                    }
                    break;
                case 'c':
                    out(info, (char)(va_arg(va, int)));
                    break;
                case 's':
                    p = va_arg(va, char *);
                    break;
                case '%':
                    out(info, '%');
                default:
                    break;
            }

            *info->bf = 0;
            info->bf = p;
            while (*info->bf++ && width > 0)
                width--;
            while (width-- > 0)
                info->putc(info, lz ? '0' : ' ');
            if (p) {
                while ((ch = *p++))
                    info->putc(info, ch);
            }
        }
    }

    abort:
    return 0;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    struct printf_info info;
    va_list va;
    int ret;

    va_start(va, fmt);
    info.outstr = buf;
    info.putc = putc_outstr;
    ret = _vprintf(&info, fmt, va);
    va_end(va);
    *info.outstr = '\0';

    return ret;
}

