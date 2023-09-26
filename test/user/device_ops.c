#include "../user.h"

#define CHECK(ret) \
    if (ret < 0) { \
        switch (errno) { \
            case ENODEV: \
                printf("Device not found\n"); \
                exit(EXIT_FAILURE);           \
            case EINVAL: \
                printf("Invalid parameters\n"); \
                exit(EXIT_FAILURE);   \
            default: \
                printf("Unknown error\n");    \
                exit(EXIT_FAILURE); \
        }          \
    } else {       \
        printf("OK\n");   \
    }

void orc_fp(){
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device: ");
    fd = open(DEVICE_PATH, O_RDONLY);
    CHECK(fd)

    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device: ");

    ret = read(fd, buf, 400);
    CHECK(ret)
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);
    memset(buf, 0, 400);

    ret = read(fd, buf, 400);
    CHECK(ret)
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);

    memset(buf, 0, 400);

    ret = read(fd, buf, 400);
    CHECK(ret)
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);

    free(buf);

    printf("3. Closing the device: ");
    ret = close(fd);
    CHECK(ret)
}

void orc(){
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device: ");
    fd = open(DEVICE_PATH, O_RDONLY);
    CHECK(fd)

    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device: ");
    ret = read(fd, buf, DEVICE_SIZE);
    CHECK(ret)
    printf("\t%d bytes read.\n", ret); //Retrieved message is: %s\n", ret, buf);

    free(buf);

    printf("3. Closing the device: ");
    ret = close(fd);
    CHECK(ret)
}

void* multi_orc(){
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device: ");
    fd = open(DEVICE_PATH, O_RDONLY);
    CHECK(fd)

    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }

    printf("2. Reading the device: ");
    ret = read(fd, buf, DEVICE_SIZE);
    CHECK(ret)
    printf("\t%d bytes read.\n", ret);

    free(buf);

    printf("3. Closing the device: ");
    ret = close(fd);
    CHECK(ret)

    pthread_exit(0);
}
