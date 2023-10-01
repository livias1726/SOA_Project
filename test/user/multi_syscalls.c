#include "../user.h"

char* msgs[] = {LOREM, EMERALD, LUCIFER};
int sizes[] = {SIZE_LOREM, SIZE_EMERALD, SIZE_LUCIFER};

void* multi_put_data(void *arg){
    int ret, size, tid = *(int*)arg, idx = rand()%3;
    char *msg;

    msg = malloc(sizes[idx]);
    memcpy(msg, msgs[idx], sizes[idx]);
    size = strlen(msg);

    ret = syscall(put, msg, size);
    if(ret < 0) {
        check_error(tid, "PUT");
        pthread_exit((void*)-1);
    } else {
        printf("[%d] - PUT on %d\n", tid, ret);
        pthread_exit(0);
    }
}

void* multi_get_data(void *arg) {
    int ret, tid = *(int*)arg, block;

    char msg[SIZE_LUCIFER];

    block = (tid%NBLOCKS)+2;
    ret = syscall(get, block, msg, SIZE_LUCIFER);
    if(ret < 0) {
        check_error(tid, "GET");
        pthread_exit((void*)-1);
    } else {
        printf("[%d] - GET from %d (%.*s)\n", tid, block, 10, msg);
        pthread_exit(0);
    }
}

void* multi_invalidate_data(void *arg){
    int ret, tid = *(int*)arg, block;

    block = (tid%NBLOCKS)+2;

    ret = syscall(inv, block);
    if(ret < 0) {
        check_error(tid, "INV");
        if (errno == EAGAIN) printf("[%d] - INV on %d RETRY\n", tid, block);
        pthread_exit((void*)-1);
    } else {
        printf("[%d] - INV on %d\n", tid, block);
        pthread_exit(0);
    }
}