#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

/* number[size - 1] = msb 32 bits, number[0] = lsb 32 bits*/
typedef struct _bn {
    unsigned int *number;
    /* the length of array: number */
    unsigned int size;
    /* 0 means positive number, 1 means negative number */
    int sign;
} bn;

char *bnToString(bn *src);
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
void bnSub(bn *a, bn *b, bn *c);

bn *bnAlloc(unsigned int size);
int bnCpy(bn *dst, bn *src);
int bnFree(bn *src);
void bnDoMul(bn *dst, int offset, unsigned long long int x);
void bnSwap(bn *a, bn *b);
void bnMul(bn *a, bn *b, bn *c);
void bnShiftL(bn *src, unsigned int shift);
void bnFib(bn *dst, unsigned int n);
void bnFibFastd(bn *dst, unsigned int n);