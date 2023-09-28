#include "../user.h"

void orc_fp(){
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device: ");
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        check_error(fd, "Open");
        return;
    }

    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device: ");

    ret = read(fd, buf, 400);
    if (ret < 0) check_error(fd, "Read");
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);
    memset(buf, 0, 400);

    ret = read(fd, buf, 400);
    if (ret < 0) check_error(fd, "Read");
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);

    memset(buf, 0, 400);

    ret = read(fd, buf, 400);
    if (ret < 0) check_error(fd, "Read");
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);

    free(buf);

    printf("3. Closing the device: ");
    ret = close(fd);
    if (ret < 0) check_error(fd, "Close");
}

void orc(){
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
    ret = read(fd, buf, 64 * 1024/*DEVICE_SIZE*/); /*TODO*/
    if (ret < 0) {
        check_error(fd, "Read");
        free(buf);
        return;
    }
    printf("%d bytes read.\n", ret); //Retrieved message is: %s\n", ret, buf);

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
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device: ");
    fd = open(DEVICE_PATH, O_RDONLY);
    if (fd < 0) {
        check_error(fd, "Open");
        pthread_exit((void*)-1);
    }

    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device: ");
    ret = read(fd, buf, DEVICE_SIZE);
    if (ret < 0) check_error(fd, "Read");
    printf("\t%d bytes read.\n", ret);

    free(buf);

    printf("3. Closing the device: ");
    ret = close(fd);
    if (ret < 0) check_error(fd, "Close");

    pthread_exit(0);
}
