#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    // long long fib, fib_fastd, fib_vla, fib_bigNum;

    char buf[256];  // 1, 256
    char write_buf[1];
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);

    struct timespec start, end;

    char *filename = "performance.txt";

    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error opening the file %s", filename);
        return -1;
    }

    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    /* VLA vs iterative vs fast-d*/
    // for (int i = 0; i <= offset; i++) {
    //     lseek(fd, i, SEEK_SET);

    //     fib_vla = write(fd, write_buf, 0);//fib VLA
    //     fib = write(fd, write_buf, 1);//fib
    //     fib_fastd = write(fd, write_buf, 2);//fib_fastdoubling

    //     fprintf(fp, "%d %lld %lld %lld\n", i, fib_vla, fib, fib_fastd);
    // }

    /* user space, kernel space, system call overhead*/
    // for (int i = 0; i <= offset; i++) {
    //     lseek(fd, i, SEEK_SET);

    //     clock_gettime(CLOCK_MONOTONIC, &start);
    //     fib_bigNum = write(fd, write_buf, 0);//fib VLA
    //     clock_gettime(CLOCK_MONOTONIC, &end);

    //     long long ut = (long long)((end.tv_sec * 1e9 + end.tv_nsec)- \
    //                  (start.tv_sec * 1e9 + start.tv_nsec));
    //     fprintf(fp, "%d %lld %lld %lld\n", i, ut, fib_bigNum, ut-fib_bigNum);
    // }

    /* kernel to user, user space, kernel space*/
    for (int i = 0; i <= offset; i++) {
        long long sz;
        long long k_to_u, kt;

        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, buf, sizeof(buf));
        clock_gettime(CLOCK_MONOTONIC, &end);
        buf[sz] = 0;

        k_to_u = write(fd, write_buf, 4);  // k_to_u
        kt = write(fd, write_buf, 5);      // kt

        long long ut = (long long) ((end.tv_sec * 1e9 + end.tv_nsec) -
                                    (start.tv_sec * 1e9 + start.tv_nsec));
        long long k_to_ut = (end.tv_sec * 1e9 + end.tv_nsec) - k_to_u;
        fprintf(fp, "%d %lld %lld %lld\n", i, ut, kt, k_to_ut);

        printf("Reading from " FIB_DEV  //
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    fclose(fp);

    close(fd);
    return 0;
}