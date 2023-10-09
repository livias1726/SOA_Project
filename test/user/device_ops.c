#include "../user.h"

void orc_fp(){
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device... ");
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        check_error(fd, "Open");
        return;
    }
    printf("OK\n");

    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device... ");

    for (int i = 0; i < 3; ++i) {
        ret = read(fd, buf, 400);
        if (ret < 0) {
            check_error(fd, "Read");
            goto fail;
        }
        printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);
    }

fail:
    free(buf);

    printf("3. Closing the device... ");
    ret = close(fd);
    if (ret < 0) {
        check_error(fd, "Close");
    } else {
        printf("OK\n");
    }
}

void orc(){
    int fd, count, ret, tot;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device... ");
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        check_error(fd, "Open");
        return;
    }
    printf("OK\n");

    count = DEVICE_SIZE;
    buf = malloc(count);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device... ");
    ret = -1;
    tot = 0;
    while(ret != 0 && tot < count) {
        ret = read(fd, buf, count-tot);
        if (ret < 0) {
            check_error(fd, "Read");
            free(buf);
            return;
        }
        printf("%d bytes read.\n", ret); //Retrieved message is: %s\n", ret, buf);
        tot += ret;
    }

    free(buf);

    printf("3. Closing the device... ");
    ret = close(fd);
    if (ret < 0) {
        check_error(fd, "Close");
    } else {
        printf("OK\n");
    }
}

void* multi_orc(){
    orc();
    pthread_exit(0);
}
