#include "user.h"

void check_error(){
    switch(errno){
        case ENODEV:
            printf("Device not mounted.\n");
            break;
        case EINVAL:
            printf("Input parameters are invalid.\n");
            break;
        case ENOMEM:
            printf("Unavailable memory on device.\n");
            break;
        case EIO:
            printf("Couldn't read device block.\n");
            break;
        case ENODATA:
            printf("Unavailable data.\n");
            break;
    }
}

void test_put_data(int sysno){
    int ret, size;
    char *msg;

    size = strlen(example_txt);
    msg = malloc(size+1);
    memcpy(msg, example_txt, size);
    size = strlen(msg);

    printf("Test parameters:\n\tsource = \"%s\"\n\tsize = %d\n", msg, size);
    ret = syscall(sysno, msg, size);
    if(ret < 0) {
        check_error();
    } else {
        printf("Message correctly written in block %d\n", ret);
    }
}

void test_get_data(int sysno) {
    int ret, size, block;
    char* msg;

    printf("Insert parameters:\n - Block index: ");
    scanf("%d", &block);
    printf(" - Size to read (in bytes): ");
    scanf("%d", &size);

    msg = malloc(size);
    if (!msg) {
        perror("malloc failed.");
        return;
    }
    ret = syscall(sysno, block, msg, size);
    if(ret < 0) {
        check_error();
    } else {
        printf("Retrieved %d bytes: \"%s\"\n", ret, msg);
    }

    free(msg);
}

void test_invalidate_data(int sysno){
    int ret, block;

    printf("Insert parameters:\n - Block index: ");
    scanf("%d", &block);

    ret = syscall(sysno, block);
    if(ret < 0) {
        check_error();
    } else {
        printf("Block %d invalidated.\n", block);
    }
}

int check_input(int argc, char *argv[], int *put, int *get, int *inv){
    if (argc < 4) {
        printf("Usage: ./user <PUT code> <GET code> <INVALIDATE code>");
        return -1;
    }

    int ret;

    *put = atoi(argv[1]);
    *get = atoi(argv[2]);
    *inv = atoi(argv[3]);

    // test for syscalls existence with invalid params to return a known error
    ret = syscall(*put, NULL, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to PUT returned with error. System call not installed.\n");

    ret = syscall(*get, -1, NULL, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to GET returned with error. System call not installed.\n");
/*
    ret = syscall(*inv, -1);
    perror("inv\n");
    //if(ret == -1 && errno == ENOSYS) printf("Test to INVALIDATE returned with error. System call not installed.\n");
*/
    return 0;
}

int main(int argc, char *argv[]){

    int put, get, inv;
    if (check_input(argc, argv, &put, &get, &inv)) return -1;

    char c;
    int i;
    printf("Choose an operation:\n"
           "\t[1] Put data\n"
           "\t[2] Get data\n"
           "\t[3] Invalidate data\n"
           "\t[other] Exit\n");

    while(1) {
        c = getc(stdin);
        i = atoi(&c);
        switch(i){
            case 1:
                test_put_data(put);
                break;
            case 2:
                test_get_data(get);
                break;
            case 3:
                test_invalidate_data(inv);
                break;
            default:
                return 0;
        }
    }
}