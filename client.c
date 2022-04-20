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
    long long sz;

    char buf[256];  // 1, 256
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);

    struct timespec start, end;

    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);

        clock_gettime(CLOCK_MONOTONIC, &start);
        sz = read(fd, buf, 1);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
        // printf("%lld\n", (long long)((end.tv_sec * 1e9 + end.tv_nsec)- \
        //         (start.tv_sec * 1e9 + start.tv_nsec)));
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    // for (int i = 0; i <= offset; i++) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, sizeof(buf));
    //     buf[sz] = 0;
    //     printf("Reading from " FIB_DEV //
    //            " at offset %d, returned the sequence "
    //            "%s.\n",
    //            i, buf);
    // }

    // for (int i = offset; i >= 0; i--) {
    //     lseek(fd, i, SEEK_SET);
    //     sz = read(fd, buf, sizeof(buf));
    //     buf[sz] = 0;
    //     printf("Reading from " FIB_DEV
    //            " at offset %d, returned the sequence "
    //            "%s.\n",
    //            i, buf);
    // }

    close(fd);
    return 0;
}