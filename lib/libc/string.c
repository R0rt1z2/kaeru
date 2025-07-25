/*
 * Copyright (c) 2005-2018 Rich Felker
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <lib/string.h>

int memcmp(const void* s1, const void* s2, size_t n) {
    if (n != 0) {
        const unsigned char *p1 = s1, *p2 = s2;

        do {
            if (*p1++ != *p2++) return (*--p1 - *--p2);
        } while (--n != 0);
    }
    return (0);
}

void* memmove(void* dest, const void* src, size_t count) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d < s || d >= s + count) {
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

void* memchr(const void* s, int c, size_t n) {
    if (n != 0) {
        const unsigned char* p = s;

        do {
            if (*p++ == (unsigned char)c) return ((void*)(p - 1));
        } while (--n != 0);
    }
    return (NULL);
}

void* memset(void* dst, int c, size_t n) {
    if (n != 0) {
        unsigned char* d = dst;

        do *d++ = (unsigned char)c;
        while (--n != 0);
    }
    return (dst);
}

char* strchr(const char* p, int ch) {
    for (;; ++p) {
        if (*p == (char)ch) return ((char*)p);
        if (!*p) return ((char*)NULL);
    }
    /* NOTREACHED */
}

static char* twobyte_strstr(const unsigned char* h, const unsigned char* n) {
    uint16_t nw = n[0] << 8 | n[1], hw = h[0] << 8 | h[1];
    for (h++; *h && hw != nw; hw = hw << 8 | *++h)
        ;
    return *h ? (char*)h - 1 : 0;
}

static char* threebyte_strstr(const unsigned char* h, const unsigned char* n) {
    uint32_t nw = n[0] << 24 | n[1] << 16 | n[2] << 8;
    uint32_t hw = h[0] << 24 | h[1] << 16 | h[2] << 8;
    for (h += 2; *h && hw != nw; hw = (hw | *++h) << 8)
        ;
    return *h ? (char*)h - 2 : 0;
}

static char* fourbyte_strstr(const unsigned char* h, const unsigned char* n) {
    uint32_t nw = n[0] << 24 | n[1] << 16 | n[2] << 8 | n[3];
    uint32_t hw = h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3];
    for (h += 3; *h && hw != nw; hw = hw << 8 | *++h)
        ;
    return *h ? (char*)h - 3 : 0;
}

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define BITOP(a, b, op) \
    ((a)[(size_t)(b) / (8 * sizeof *(a))] op(size_t) 1 << ((size_t)(b) % (8 * sizeof *(a))))

/*
 * Maxime Crochemore and Dominique Perrin, Two-way string-matching,
 * Journal of the ACM, 38(3):651-675, July 1991.
 */
static char* twoway_strstr(const unsigned char* h, const unsigned char* n) {
    const unsigned char* z;
    size_t l, ip, jp, k, p, ms, p0, mem, mem0;
    size_t byteset[32 / sizeof(size_t)] = {0};
    size_t shift[256];

    /* Computing length of needle and fill shift table */
    for (l = 0; n[l] && h[l]; l++) BITOP(byteset, n[l], |=), shift[n[l]] = l + 1;
    if (n[l]) return 0; /* hit the end of h */

    /* Compute maximal suffix */
    ip = -1;
    jp = 0;
    k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) {
            if (k == p) {
                jp += p;
                k = 1;
            } else
                k++;
        } else if (n[ip + k] > n[jp + k]) {
            jp += k;
            k = 1;
            p = jp - ip;
        } else {
            ip = jp++;
            k = p = 1;
        }
    }
    ms = ip;
    p0 = p;

    /* And with the opposite comparison */
    ip = -1;
    jp = 0;
    k = p = 1;
    while (jp + k < l) {
        if (n[ip + k] == n[jp + k]) {
            if (k == p) {
                jp += p;
                k = 1;
            } else
                k++;
        } else if (n[ip + k] < n[jp + k]) {
            jp += k;
            k = 1;
            p = jp - ip;
        } else {
            ip = jp++;
            k = p = 1;
        }
    }
    if (ip + 1 > ms + 1)
        ms = ip;
    else
        p = p0;

    /* Periodic needle? */
    if (memcmp(n, n + p, ms + 1)) {
        mem0 = 0;
        p = MAX(ms, l - ms - 1) + 1;
    } else
        mem0 = l - p;
    mem = 0;

    /* Initialize incremental end-of-haystack pointer */
    z = h;

    /* Search loop */
    for (;;) {
        /* Update incremental end-of-haystack pointer */
        if (z - h < l) {
            /* Fast estimate for MIN(l,63) */
            size_t grow = l | 63;
            const unsigned char* z2 = memchr(z, 0, grow);
            if (z2) {
                z = z2;
                if (z - h < l) return 0;
            } else
                z += grow;
        }

        /* Check last byte first; advance by shift on mismatch */
        if (BITOP(byteset, h[l - 1], &)) {
            k = l - shift[h[l - 1]];
            if (k) {
                if (k < mem) k = mem;
                h += k;
                mem = 0;
                continue;
            }
        } else {
            h += l;
            mem = 0;
            continue;
        }

        /* Compare right half */
        for (k = MAX(ms + 1, mem); n[k] && n[k] == h[k]; k++)
            ;
        if (n[k]) {
            h += k - ms;
            mem = 0;
            continue;
        }
        /* Compare left half */
        for (k = ms + 1; k > mem && n[k - 1] == h[k - 1]; k--)
            ;
        if (k <= mem) return (char*)h;
        h += p;
        mem = mem0;
    }
}

char* strstr(const char* h, const char* n) {
    /* Return immediately on empty needle */
    if (!n[0]) return (char*)h;

    /* Use faster algorithms for short needles */
    h = strchr(h, *n);
    if (!h || !n[1]) return (char*)h;
    if (!h[1]) return 0;
    if (!n[2]) return twobyte_strstr((void*)h, (void*)n);
    if (!h[2]) return 0;
    if (!n[3]) return threebyte_strstr((void*)h, (void*)n);
    if (!h[3]) return 0;
    if (!n[4]) return fourbyte_strstr((void*)h, (void*)n);

    return twoway_strstr((void*)h, (void*)n);
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 == *s2++)
        if (*s1++ == 0) return (0);
    return (*(unsigned char*)s1 - *(unsigned char*)--s2);
}

int strncmp(const char* s1, const char* s2, size_t n) {
    if (n == 0) return (0);
    do {
        if (*s1 != *s2++) return (*(unsigned char*)s1 - *(unsigned char*)--s2);
        if (*s1++ == 0) break;
    } while (--n != 0);
    return (0);
}

size_t strlen(const char* str) {
    const char* s;

    for (s = str; *s; ++s)
        ;
    return (s - str);
}

unsigned short strtou16(const char* str) {
    unsigned short result = 0;

    while (*str && ISSPACE(*str)) {
        str++;
    }

    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }

    return result;
}

char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++));
    return original_dest;
}

char *strncpy(char *dest, char const *src, size_t count) {
	char *tmp = dest;
    while(count-- && (*dest++ = *src++) != '\0')
    ;
	return tmp;
}