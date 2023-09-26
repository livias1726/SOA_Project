#include "user.h"

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
    printf("\t%d bytes read. Retrieved message is: %s\n", ret, buf);

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

int main(){
    int i;
    pthread_t tids[THREADS_PER_CALL];

    printf("Choose a test on file system operations:\n"
           "\t[1] ST Open-Read-Close\n"
           "\t[2] ST Open-(Multi)Read-Close\n"
           "\t[3] MT Open-Read-Close\n"
           "\t[other] Exit\n");

    switch(getint()){
        case 1:
            orc();
            break;
        case 2:
            orc_fp();
            break;
        case 3:
            pthread_barrier_init(&barrier, NULL, THREADS_PER_CALL);

            for (i = 0; i < THREADS_PER_CALL; ++i) pthread_create(&tids[i], NULL, multi_orc, NULL);
            for (i = 0; i < THREADS_PER_CALL; ++i) pthread_join(tids[i], NULL);

            pthread_barrier_destroy(&barrier);
            break;
        default:
            break;
    }

    return 0;
}
