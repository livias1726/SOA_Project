#include "user.h"

#define CHECK(ret) \
    if (ret < 0) { \
        switch (errno) { \
            case ENODEV: \
                printf("Device not found\n"); \
                exit(EXIT_FAILURE);   \
            default: \
                printf("Unknown error\n");    \
        }          \
    } else {       \
        printf("OK\n");   \
    }

int main(){
    int fd, ret;
    char *buf;

    printf("Starting device operations tests...\n");

    printf("1. Opening the device: ");
    fd = open(DEVICE_PATH, O_RDONLY);
    CHECK(fd)

    printf("2. Reading the device: ");
    buf = malloc(DEVICE_SIZE);
    if (!buf) {
        perror("Malloc failed.");
        exit(EXIT_FAILURE);
    }
    ret = read(fd, buf, DEVICE_SIZE);
    CHECK(ret)
    printf("\tDevice content is: %s\n", buf);
    free(buf);

    printf("3. Closing the device: ");
    ret = close(fd);
    CHECK(ret)

    exit(EXIT_SUCCESS);
}
