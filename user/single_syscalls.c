#include "user.h"

void test_put_data(){
    int ret, size;
    char *msg;

    msg = malloc(SIZE_LOREM);
    memcpy(msg, LOREM, SIZE_LOREM);
    size = strlen(msg);

    printf("Test parameters:\n\tsource = \"%s\"\n\tsize = %d\n", msg, size);
    ret = syscall(put, msg, size);
    if(ret < 0) {
        check_error(0);
    } else {
        printf("Message correctly written in block %d\n", ret);
    }

    free(msg);
}

void test_get_data() {
    int ret, size, block;
    char* msg;

    printf("Which block do you want to read (indexes starts from 2)? ");
    block = getint();
    printf("How many bytes do you want to read? ");
    size = getint();

    msg = malloc(size);
    if (!msg) {
        perror("malloc failed.");
        return;
    }

    ret = syscall(get, block, msg, size);
    if(ret < 0) {
        check_error(0);
    } else {
        printf("Retrieved %d bytes: \"%s\"\n", ret, msg);
    }

    free(msg);
}

void test_invalidate_data(){
    int ret, block;

    printf("Which block do you want to invalidate (indexes starts from 2)? ");
    block = getint();

    ret = syscall(inv, block);
    if(ret < 0) {
        check_error(0);
    } else {
        printf("Block %d invalidated.\n", block);
    }
}

int main(int argc, char *argv[]){

    int i;

    if (check_input(argc, argv)) return -1;

    while(1) {

        printf("Choose an operation:\n"
               "\t[1] Put data\n"
               "\t[2] Get data\n"
               "\t[3] Invalidate data\n"
               "\t[other] Exit\n");
        i = getint();

        switch(i){
            case 1:
                test_put_data();
                break;
            case 2:
                test_get_data();
                break;
            case 3:
                test_invalidate_data();
                break;
            default:
                return 0;
        }
    }
}