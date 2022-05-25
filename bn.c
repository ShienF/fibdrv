#include "bn.h"

#define __swap(a, b)         \
    do {                     \
        typeof(a) __tmp = a; \
        a = b;               \
        b = __tmp;           \
    } while (0)

#define __max(a, b) ((a) > (b) ? (a) : (b))

#define divRoundUp(a, len) (((a) + (len) -1) / (len))


char *bnToString(bn *src)
{
    /* log10(x) = log2(x) / log2(10)
     * log2(10) ~= 3.~
     * log2(x) = totals bits in binary
     * so log10(x) ~= totals bits in binary / 3 + 1
     * one more bit for '\0' */
    size_t len = (8 * sizeof(unsigned int) * src->size) / 3 + 2 + src->sign;
    char *s = (char *) kmalloc(len, GFP_KERNEL);
    char *p = s;

    /* initialize decimal string*/
    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    /* binary to decimal string*/
    for (int i = src->size - 1; i >= 0; i--) {
        for (unsigned int check = 1U << 31; check; check >>= 1) {
            int carry = !!(check & src->number[i]);
            for (int j = len - 2; j >= 0; j--) {
                s[j] += s[j] - '0' + carry;
                carry = (s[j] > '9');
                if (carry) {
                    s[j] -= 10;
                }
            }
        }
    }

    /* skip leading zeros */
    while (p[0] == '0' && p[1] != '\0') {  ///
        p++;
    }
    if (src->sign)
        *(--p) = '-';
    memmove(s, p, strlen(p) + 1);

    return s;
}

int bnResize(bn *src, unsigned int size)
{
    if (!src)
        return -1;

    if (size == src->size)
        return 0;

    src->number =
        krealloc(src->number, sizeof(unsigned int) * size, GFP_KERNEL);

    if (!src->number)
        return -1;

    if (size > src->size)
        memset(src->number + src->size, 0,
               sizeof(unsigned int) * (size - src->size));

    src->size = size;
    return 0;
}

int bnClz(bn *a)
{
    int cnt = 0;
    for (int i = a->size - 1; i >= 0; i--) {
        if (a->number[i])
            return (cnt += __builtin_clz(a->number[i]));
        else
            cnt += 32;
    }

    return cnt;
}

int digitsAtmsb(bn *a)
{
    return a->size * 32 - bnClz(a);
}

/* Compare length of |a| and |b|*/
int bnCmp(bn *a, bn *b)
{
    if (a->size > b->size)
        return 1;
    else if (a->size < b->size)
        return -1;
    else {
        for (int i = a->size - 1; i >= 0; i--) {
            if (a->number[i] > b->number[i])
                return 1;
            if (a->number[i] < b->number[i])
                return -1;
        }
        return 0;
    }
}

/* the sign of a and b are the same */
void bnDoAdd(bn *a, bn *b, bn *c)
{
    int d = __max(digitsAtmsb(a), digitsAtmsb(b)) + 1;
    d = divRoundUp(d, 32);
    bnResize(c, d);

    unsigned long long int carry = 0;  // 64 bits

    for (int i = 0; i < c->size; i++) {
        unsigned int tmpA = i < a->size ? a->number[i] : 0;
        unsigned int tmpB = i < b->size ? b->number[i] : 0;
        carry += (unsigned long long int) tmpA + tmpB;  ///
        c->number[i] = carry;
        carry >>= 32;
    }

    if (!c->number[c->size - 1] && c->size > 1)
        bnResize(c, c->size - 1);
}

/* |a| > |b| */
void bnDoSub(bn *a, bn *b, bn *c)
{
    bnResize(c, a->size);

    long long int carry = 0;

    for (int i = 0; i < c->size; i++) {
        unsigned int tmpA = i < a->size ? a->number[i] : 0;
        unsigned int tmpB = i < b->size ? b->number[i] : 0;
        carry = (long long int) tmpA - tmpB - carry;
        c->number[i] = carry;
        if (carry < 0) {
            carry = 1;
        } else {
            carry = 0;
        }
    }

    int d = bnClz(c) / 32;
    if (d == c->size)
        d--;
    bnResize(c, c->size - d);
}

/* c = a + b */
void bnAdd(bn *a, bn *b, bn *c)
{
    /* a,b > 0 or a,b < 0 */
    if (a->sign == b->sign) {
        bnDoAdd(a, b, c);
        c->sign = a->sign;
    }
    /* a and b have different sign */
    else {
        if (a->sign)
            __swap(a, b);
        int cmp = bnCmp(a, b);
        /* |a| > |b|, a > 0, b < 0, a + b --> |a| - |b| */
        if (cmp == 1) {
            bnDoSub(a, b, c);
            c->sign = 0;
        }
        /* |a| < |b|, a < 0, b > 0, a + b --> -(|b| - |a|) */
        else if (cmp == -1) {
            bnDoSub(b, a, c);
            c->sign = 1;
        }
        /* |a| = |b| */
        else {
            bnResize(c, 1);
            c->number[0] = 0;
            c->sign = 0;
        }
    }
}

/* c = a - b,
 * it can convert to a + (-b),
 * so inverse the sign of b,
 * and put them into bnAdd() */
void bnSub(bn *a, bn *b, bn *c)
{
    bn tmp = *b;
    tmp.sign ^= 1;
    bnAdd(a, &tmp, c);
}

bn *bnAlloc(unsigned int size)
{
    bn *new = (bn *) kmalloc(sizeof(bn), GFP_KERNEL);
    new->number =
        (unsigned int *) kmalloc(sizeof(unsigned int) * size, GFP_KERNEL);
    memset(new->number, 0, sizeof(unsigned int) * size);
    new->size = size;
    new->sign = 0;
    return new;
}

int bnCpy(bn *dst, bn *src)
{
    if (bnResize(dst, src->size) < 0)
        return -1;

    memcpy(dst->number, src->number, sizeof(unsigned int) * src->size);
    dst->sign = src->sign;
    return 0;
}

int bnFree(bn *src)
{
    if (src == NULL)
        return -1;

    kfree(src->number);
    kfree(src);
    return 0;
}

void bnDoMul(bn *dst, int offset, unsigned long long int x)
{
    unsigned long long int carry = 0;
    for (int i = offset; i < dst->size; i++) {
        carry += dst->number[i] + (x & 0xffffffff);  // prevent overflow
        dst->number[i] = carry;
        carry >>= 32;
        x >>= 32;

        if (!carry && !x) {
            return;
        }
    }
}

void bnSwap(bn *a, bn *b)
{
    bn tmp = *a;
    *a = *b;
    *b = tmp;
}

void bnMul(bn *a, bn *b, bn *c)
{
    int d = digitsAtmsb(a) + digitsAtmsb(b);
    d = divRoundUp(d, 32);
    bn *tmp;

    if (a == c || b == c) {
        /* save c */
        tmp = c;
        c = bnAlloc(d);  ///
    } else {
        tmp = NULL;
        bnResize(c, d);
    }

    for (int i = 0; i < a->size; i++) {
        for (int j = 0; j < b->size; j++) {
            unsigned long long int carry = 0;
            carry = (unsigned long long int) a->number[i] * b->number[j];  ///
            bnDoMul(c, i + j, carry);
        }
    }
    c->sign = a->sign ^ b->sign;

    /* a == b or a == c case */
    if (tmp) {
        bnSwap(tmp,
               c);  /// address should be swap, so bnCpy(tmp, c) doesn't work
        bnFree(c);
    }
}

void bnShiftL(bn *src, unsigned int shift)
{
    if (bnClz(src) < shift)
        bnResize(src, src->size + 1);

    for (int i = src->size - 1; i; i--) {
        src->number[i] =
            src->number[i] << shift | src->number[i - 1] >> (32 - shift);
    }

    src->number[0] <<= shift;
}

void bnFib(bn *dst, unsigned int n)
{
    bnResize(dst, 1);

    /* fib(0) = 0, fib(1) = 1 */
    if (n < 2) {
        dst->number[0] = n;
        return;
    }

    /* starts from fib(2) */
    bn *a = bnAlloc(1);
    bn *b = bnAlloc(1);
    b->number[0] = 1U;

    for (int i = 2; i <= n; i++) {
        bnAdd(a, b, dst);
        bnCpy(a, dst);
        bnSwap(a, b);
    }

    bnFree(a);
    bnFree(b);
}

void bnFibFastd(bn *dst, unsigned int n)
{
    bnResize(dst, 1);

    /* fib(0) = 0, fib(1) = 1 */
    if (n < 2) {
        dst->number[0] = n;
        return;
    }

    bn *a = dst;         // fib(k)
    bn *b = bnAlloc(1);  // fib(k+1)
    a->number[0] = 0;
    b->number[0] = 1U;

    bn *t1 = bnAlloc(1);  // fib(2k)
    bn *t2 = bnAlloc(1);  // fib(2k+1)

    unsigned int bits = 0;
    for (unsigned int i = n; i; bits++, i >>= 1)
        ;

    for (unsigned int i = bits; i; i -= 1) {
        /* fib(2k) = fib(k) * (2 * fib(k+1) - fib(k)) */
        bnCpy(t1, b);
        bnShiftL(t1, 1);  // 2 * fib(k+1)
        bnSub(t1, a, t1);
        bnMul(t1, a, t1);

        /* fib(2k+1) = fib(k+1)^2 + fib(k)^2 */
        bnCpy(t2, b);
        bnMul(t2, t2, t2);
        bnMul(a, a, a);
        bnAdd(t2, a, t2);

        if (n & (1UL << (i - 1))) {
            bnAdd(t1, t2, t1);
            bnCpy(a, t2);
            bnCpy(b, t1);
        } else {
            bnCpy(a, t1);
            bnCpy(b, t2);
        }
    }

    bnFree(b);
    bnFree(t1);
    bnFree(t2);
}