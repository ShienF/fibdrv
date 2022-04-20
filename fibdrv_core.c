#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "xs.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 100  // 92

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static ktime_t kt;
static ktime_t k_to_u;

#define XOR_SWAP(a, b, type) \
    do {                     \
        type *__c = (a);     \
        type *__d = (b);     \
        *__c ^= *__d;        \
        *__d ^= *__c;        \
        *__c ^= *__d;        \
    } while (0)

static void __swap(void *a, void *b, size_t size)
{
    if (a == b)
        return;

    switch (size) {
    case 1:
        XOR_SWAP(a, b, char);
        break;
    case 2:
        XOR_SWAP(a, b, short);
        break;
    case 4:
        XOR_SWAP(a, b, unsigned int);
        break;
    case 8:
        XOR_SWAP(a, b, unsigned long);
        break;
    default:
        /* Do nothing */
        break;
    }
}

static void reverse_str(char *str, size_t n)
{
    for (int i = 0; i < (n >> 1); i++)
        __swap(&str[i], &str[n - i - 1], sizeof(char));
}

static void string_number_add(xs *a, xs *b, xs *out)
{
    char *data_a, *data_b;
    size_t size_a, size_b;
    int i, carry = 0;
    int sum;

    /*
     * Make sure the string length of 'a' is always greater than
     * the one of 'b'.
     */
    if (xs_size(a) < xs_size(b))
        __swap((void *) &a, (void *) &b, sizeof(void *));

    data_a = xs_data(a);
    data_b = xs_data(b);

    size_a = xs_size(a);
    size_b = xs_size(b);

    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    char buf[size_a + 2];

    for (i = 0; i < size_b; i++) {
        sum = (data_a[i] - '0') + (data_b[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    for (i = size_b; i < size_a; i++) {
        sum = (data_a[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    if (carry)
        buf[i++] = '0' + carry;

    buf[i] = 0;

    reverse_str(buf, i);

    /* Restore the original string */
    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    if (out)
        *out = *xs_tmp(buf);
}

static long long fib_sequence_vla(long long k)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    long long f[k + 2];

    f[0] = 0;
    f[1] = 1;

    for (int i = 2; i <= k; i++) {
        f[i] = f[i - 1] + f[i - 2];
    }

    return f[k];
}

long long fib_sequence(long long k)
{
    if (k < 2) {
        return k;
    }

    long long k1 = 0, k2 = 1;

    for (int i = 2; i <= k; i++) {
        long long sum;
        sum = k1 + k2;
        k1 = k2;
        k2 = sum;
    }

    return k2;
}

long long fib_sequence_fastd(long long k)
{
    if (k < 2) {
        return k;
    }

    int bits = 0;
    for (unsigned int i = k; i; bits++, i >>= 1)
        ;

    long long k1 = 0, k2 = 1;
    for (unsigned int i = bits; i; i -= 1) {
        long long Tk1, Tk2;
        Tk1 = k1 * (2 * k2 - k1);
        Tk2 = k2 * k2 + k1 * k1;

        k1 = Tk1;
        k2 = Tk2;

        if (k & (1UL << (i - 1))) {
            Tk1 = k1 + k2;
            k1 = k2;
            k2 = Tk1;
        }
    }

    return k1;
}

int fib_sequence_bigNum_ten(long long k, char __user *buf)
{
    int length = 1;

    if (k == 0) {
        char *tmp = "0";
        copy_to_user(buf, tmp, length);
    } else if (k == 1) {
        char *tmp = "1";
        copy_to_user(buf, tmp, length);
    } else {
        xs *k1 = (xs *) kmalloc(sizeof(xs), GFP_KERNEL),
           *k2 = (xs *) kmalloc(sizeof(xs), GFP_KERNEL),
           *sum = (xs *) kmalloc(sizeof(xs), GFP_KERNEL);  //

        *k1 = *xs_tmp("0");
        *k2 = *xs_tmp("1");

        for (int i = 2; i <= k; i++) {
            string_number_add(k1, k2, sum);
            *k1 = *xs_new(k1, xs_data(k2));
            *k2 = *xs_new(k2, xs_data(sum));
        }

        length = xs_size(k2);

        // printk(KERN_INFO  "result = %s", xs_data(k2));

        copy_to_user(buf, xs_data(k2), length);

        xs_free(k1);
        xs_free(k2);
        xs_free(sum);
    }
    return length;
}


static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */

// static ssize_t fib_read(struct file *file,
//                         char *buf,
//                         size_t size,
//                         loff_t *offset)
// {
//     return (ssize_t) fib_sequence(*offset);
// }

// static ssize_t fib_read(struct file *file,
//                         char *buf,
//                         size_t size,
//                         loff_t *offset)
// {
//     return (ssize_t) fib_sequence_fastd(*offset);
// }

static ssize_t fib_read(struct file *file,  // ssize_t: long long
                        char __user *buf,
                        size_t size,
                        loff_t *offset)  // loff_t: long long
{
    kt = ktime_get();
    ssize_t res = (ssize_t) fib_sequence_bigNum_ten(*offset, buf);
    k_to_u = ktime_get();
    kt = ktime_sub(k_to_u, kt);
    return res;
}


/* write operation is skipped
 * so use this to calculate time spent in kernel*/
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    switch (size) {
    case 0:
        kt = ktime_get();
        fib_sequence_vla(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;

    case 1:
        kt = ktime_get();
        fib_sequence(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;

    case 2:
        kt = ktime_get();
        fib_sequence_fastd(*offset);
        kt = ktime_sub(ktime_get(), kt);
        break;

    case 3:
        fib_sequence_bigNum_ten(*offset, buf);
        kt = ktime_get();
        break;

    case 4:
        return (ssize_t) ktime_to_ns(k_to_u);

    case 5:
        return (ssize_t) ktime_to_ns(kt);

    default:
        return 0;
    }
    return (ssize_t) ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
