#include "bn.h"

#define swap(a, b)           \
    do {                     \
        typeof(a) __tmp = a; \
        a = b;               \
        b = __tmp;           \
    } while (0)

#define max(a, b) (a) > (b) ? (a) : (b)

#define divRoundUp(a, len) (((a) + ((len) -1)) / (len))


int bnResize(bn *src, unsigned int size)
{
    if (!src)
        return -1;
    if (size == src->size)
        return 0;

    src->number = realloc(src->number, sizeof(unsigned int) * size);
    if (!src->number)
        return -1;

    if (size > src->size)
        src->number = memset(src->number + src->size, 0, size - src->size);

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
        if (digitsAtmsb(a) > digitsAtmsb(b))
            return 1;
        else if (digitsAtmsb(a) < digitsAtmsb(b))
            return -1;
        else
            return 0;
    }
}

/* the sign of a and b are the same */
void bnDoAdd(bn *a, bn *b, bn *c)
{
    int d = max(digitsAtmsb(a), digitsAtmsb(b)) + 1;
    d = divRoundUp(d, 32);
    bnResize(c, d);

    unsigned long long int carry = 0;  // 64 bits

    for (int i = 0; i < c->size; i++) {
        unsigned int tmpA = i < a->size ? a->number[i] : 0;
        unsigned int tmpB = i < b->size ? b->number[i] : 0;
        carry += tmpA + tmpB + carry;
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
            swap(a, b);
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

