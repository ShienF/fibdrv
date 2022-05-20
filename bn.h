#include <linux/kernel.h>
#include <linux/slab.h>

/* number[size - 1] = msb 32 bits, number[0] = lsb 32 bits*/
typedef struct _bn {
    unsigned int *number;
    /* the length of array: number */
    unsigned int size;
    /* 0 means positive number, 1 means negative number */
    int sign;
} bn;

char *bnToString(bn *src)
{
    /* log10(x) = log2(x) / log2(10)
     * log2(10) ~= 3.~
     * log2(x) = totals bits in binary
     * so log10(x) ~= totals bits in binary / 3 + 1
     * one more bit for '\0' */
    size_t len = (8 * sizeof(unsigned int) * src->size) / 3 + 2;
    char *s = (char *) kmalloc(len, GFP_KERNEL);
    char *p = s;

    /* initialize decimal string*/
    memset(s, '0', len - 1);
    s[len - 1] = '\0';

    /* binary to decimal string*/
    for (int i = src->size - 1; i >= 0; i--) {
        for (unsigned int check = 1U << 31; check > 0; check >>= 1) {
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
    while (*p == '0' && *p + 1 != '\0')
        p++;
    if (src->sign)
        *(--p) = '-';
    memmove(s, p, strlen(p) + 1);

    return s;
}

int bnResize(bn *src, unsigned int size);
int bnClz(bn *a);
int digitsAtmsb(bn *a);

/* Compare length of |a| and |b|*/
int bnCmp(bn *a, bn *b);

/* the sign of a and b are the same */
void bnDoAdd(bn *a, bn *b, bn *c);

/* |a| > |b| */
void bnDoSub(bn *a, bn *b, bn *c);

/* c = a + b */
void bnAdd(bn *a, bn *b, bn *c);

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