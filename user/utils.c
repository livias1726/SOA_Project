#include "user.h"

int getint(){
    int i = 0;
    char s[MAX_IN], c;

    while ((c = getchar()) != '\n' && i < MAX_IN-1) {
        s[i++] = c;
    }
    if (!i)
    s[i] = '\0';

    while (c != '\n') { // stdin flush
        c = getchar();
    }

    return strtol(s, NULL, 10);
}

int check_input(int argc, char *argv[]){
    int ret;

    if (argc < 4) {
        printf("Usage: ./user <PUT code> <GET code> <INVALIDATE code>");
        return -1;
    }

    // test for syscalls existence with invalid params to return a known error

    put = strtol(argv[1], NULL, 10);
    ret = syscall(put, NULL, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to PUT returned with error. System call not installed.\n");

    get = strtol(argv[2], NULL, 10);
    ret = syscall(get, -1, NULL, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to GET returned with error. System call not installed.\n");

    inv = strtol(argv[3], NULL, 10);
    ret = syscall(inv, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to INVALIDATE returned with error. System call not installed.\n");

    return 0;
}

void check_error(int tid){
    switch(errno){
        case ENODEV:
            printf("[%d] - Device not mounted.\n", tid);
            break;
        case EINVAL:
            printf("[%d] - Input parameters are invalid.\n", tid);
            break;
        case ENOMEM:
            printf("[%d] - Unavailable memory on device.\n", tid);
            break;
        case EIO:
            printf("[%d] - Couldn't read device block.\n", tid);
            break;
        case ENODATA:
            printf("[%d] - Unavailable data.\n", tid);
            break;
    }
}