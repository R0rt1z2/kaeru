#include "libc.h"

#include <stdarg.h>

/** \brief Division result
 *  \sa uidiv
 */
typedef struct uidiv_result {
    u32_t quo;  ///< Quotient
    u32_t rem;  ///< Remainder
} uidiv_result_t;

/********************************************//**
 *  \brief Unsigned integer division
 *
 *  ARM does not have native division support
 *  \returns Result of operation or zero if
 *  dividing by zero.
 ***********************************************/
uidiv_result_t
uidiv (u32_t num,   ///< Numerator
       u32_t dem)   ///< Denominator
{
    u32_t tmp = dem;
    uidiv_result_t ans = {0};

    if (dem == 0)
    {
        // TODO: Somehow make error
        return ans;
    }

    while (tmp <= num >> 1)
    {
        tmp <<= 1;
    }

    do
    {
        if (num >= tmp)
        {
            num -= tmp;
            ans.quo++;
        }
        ans.quo <<= 1;
        tmp >>= 1;
    } while (tmp >= dem);
    ans.quo >>= 1;
    ans.rem = num;

    return ans;
}

void*  memset(void*  dst, int c, u32_t n)
{
    char*  q   = dst;
    char*  end = q + n;

    for (;;) {
        if (q >= end) break; *q++ = (char) c;
        if (q >= end) break; *q++ = (char) c;
        if (q >= end) break; *q++ = (char) c;
        if (q >= end) break; *q++ = (char) c;
    }

    return dst;
}

u32_t
strlen(const char *str)
{
    const char *s;

    for (s = str; *s; ++s)
        ;
    return (s - str);
}

// thanks naehrwert for the tiny printf
static void _putn(char **p_str, u32_t x, u32_t base, char fill, int fcnt, int upper)
{
    char buf[65];
    char *digits;
    char *p;
    int c = fcnt;
    uidiv_result_t div_res;

    if (upper)
        digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    else
        digits = "0123456789abcdefghijklmnopqrstuvwxyz";

    if(base > 36)
        return;

    p = buf + 64;
    *p = 0;
    do
    {
        c--;
        div_res = uidiv (x, base);
        *--p = digits[div_res.rem];
        x = div_res.quo;
    }while(x);

    if(fill != 0)
    {
        while(c > 0)
        {
            *--p = fill;
            c--;
        }
    }

    for(; *p != '\0'; *((*p_str)++) = *(p++));
}

/********************************************//**
 *  \brief Simple @c vsprintf
 *
 *  Only supports %c, %s, %u, %x, %X with
 *  optional zero padding.
 *  Always returns zero.
 ***********************************************/
int vsprintf (char *str, const char *fmt, va_list ap)
{
    char *s;
    char c, fill;
    int fcnt;
    u32_t n;

    while(*fmt)
    {
        if(*fmt == '%')
        {
            fmt++;
            fill = 0;
            fcnt = 0;
            if((*fmt >= '0' && *fmt <= '9') || *fmt == ' ')
                if(*(fmt+1) >= '0' && *(fmt+1) <= '9')
                {
                    fill = *fmt;
                    fcnt = *(fmt+1) - '0';
                    fmt++;
                    fmt++;
                }
            switch(*fmt)
            {
                case 'c':
                    c = va_arg(ap, u32_t);
                    *(str++) = c;
                    break;
                case 's':
                    s = va_arg(ap, char *);
                    for(; *s != '\0'; *(str++) = *(s++));
                    break;
                case 'u':
                    n = va_arg(ap, u32_t);
                    _putn(&str, n, 10, fill, fcnt, 0);
                    break;
                case 'x':
                    n = va_arg(ap, u32_t);
                    _putn(&str, n, 16, fill, fcnt, 0);
                    break;
                case 'X':
                    n = va_arg(ap, u32_t);
                    _putn(&str, n, 16, fill, fcnt, 1);
                    break;
                case '%':
                    *(str++) = '%';
                    break;
                case '\0':
                    goto out;
                default:
                    *(str++) = '%';
                    *(str++) = *fmt;
                    break;
            }
        }
        else
            *(str++) = *fmt;
        fmt++;
    }

    out:
    *str = '\0';
    return 0;
}

/********************************************//**
 *  \brief Simple @c sprintf
 *
 *  Only supports %c, %s, %u, %x, %X with
 *  optional zero padding.
 *  Always returns zero.
 ***********************************************/
int sprintf (char *str, const char *format, ...)
{
    va_list arg;

    va_start (arg, format);
    vsprintf (str, format, arg);
    va_end (arg);
    return 0;
}

char *
strcpy(char *to, const char *from)
{
    char *save = to;

    for (; (*to = *from) != '\0'; ++from, ++to);
    return(save);
}

char *
strcat(char *dest, const char *src)
{
    strcpy(dest + strlen(dest), src);
    return dest;
}

/*
 * Compare strings.
 */
int
strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == 0)
            return (0);
    return (*(unsigned char *)s1 - *(unsigned char *)--s2);
}

int
strncmp(const char *s1, const char *s2, u32_t n)
{
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

void *memcpy(void *dest, const void *src, size_t n)
{
    char *dp = dest;
    const char *sp = src;
    while (n--)
        *dp++ = *sp++;
    return dest;
}

int memcmp(const void* s1, const void* s2,size_t n)
{
    const unsigned char *p1 = s1, *p2 = s2;
    while(n--)
        if( *p1 != *p2 )
            return *p1 - *p2;
        else
            p1++,p2++;
    return 0;
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