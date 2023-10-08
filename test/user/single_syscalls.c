#include "../user.h"

char* msgs[] = {lorem, emerald, lucifer};
int sizes[] = {SIZE_LOREM, SIZE_EMERALD, SIZE_LUCIFER};

void test_put_data(){
    int ret, size, idx;
    char *msg;

    printf("Choose a message:\n"
           "\t[1] Example text\n"
           "\t[2] Emerald tablet\n"
           "\t[3] Dante's Lucifer\n"
           "\t[other] Custom\n");
    idx = getint();

    switch(idx){
        case 1:
        case 2:
        case 3:
            size = sizes[--idx];
            msg = malloc(size);
            if (!msg) {
                printf("malloc failed\n");
                exit(EXIT_FAILURE);
            }
            memcpy(msg, msgs[idx], size);
            memcpy(msg + size, "\0", 1);
            break;
        default:
            printf("Insert a message: ");
            msg = getstr();
            if (!msg) {
                printf("malloc failed\n");
                exit(EXIT_FAILURE);
            }
            size = strlen(msg);
    }

    printf("Test parameters:\n\tsource = \"%s\"\n\tsize = %d\n", msg, size);
    ret = syscall(put, msg, size);
    if(ret < 0) {
        check_error(0, "PUT");
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
        check_error(0, "GET");
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
        check_error(0, "INV");
    } else {
        printf("Block %d invalidated.\n", block);
    }
}