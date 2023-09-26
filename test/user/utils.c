#include "../user.h"

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
        printf("Usage: <exe> <PUT code> <GET code> <INVALIDATE code>");
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

void check_error(int tid, char* call){
    switch(errno){
        case ENODEV:
            printf("[%s, %d] - Device not mounted.\n", call, tid);
            break;
        case EINVAL:
            printf("[%s, %d] - Input parameters are invalid.\n", call, tid);
            break;
        case ENOMEM:
            printf("[%s, %d] - Unavailable memory on device.\n", call, tid);
            break;
        case EIO:
            printf("[%s, %d] - Couldn't read device block.\n", call, tid);
            break;
        case ENODATA:
            printf("[%s, %d] - Unavailable data.\n", call, tid);
            break;
        case EAGAIN:
            printf("[%s, %d] - Try again.\n", call, tid);
            break;
    }
}