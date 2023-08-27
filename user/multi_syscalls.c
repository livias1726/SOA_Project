#include "user.h"

char* msgs[] = {LOREM, EMERALD, LUCIFER};
int sizes[] = {SIZE_LOREM, SIZE_EMERALD, SIZE_LUCIFER};

void* test_put_data(void *arg){
    int ret, size, tid = *(int*)arg, idx = rand()%3;
    char *msg;

    msg = malloc(sizes[idx]);
    memcpy(msg, msgs[idx], sizes[idx]);
    size = strlen(msg);

    ret = syscall(put, msg, size);
    if(ret < 0) {
        check_error(tid);
        pthread_exit((void*)-1);
    } else {
        printf("[%d] - PUT on %d\n", tid, ret);
        pthread_exit(0);
    }
}

void* test_get_data(void *arg) {
    int ret, tid = *(int*)arg;

    char msg[SIZE_LOREM];

    ret = syscall(get, tid+2, msg, SIZE_LOREM);
    if(ret < 0) {
        check_error(tid);
        pthread_exit((void*)-1);
    } else {
        printf("[%d] - GET from %d (%.*s)\n", tid, tid+2, 10, msg);
        pthread_exit(0);
    }
}

void* test_invalidate_data(void *arg){
    int ret, tid = *(int*)arg;

    ret = syscall(inv, tid+2);
    if(ret < 0) {
        check_error(tid);
        pthread_exit((void*)-1);
    } else {
        printf("[%d] - INV on %d\n", tid, tid+2);
        pthread_exit(0);
    }
}

void non_sequential(){
    int i, j, args[THREADS_PER_CALL];
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        args[i] = i;
        pthread_create(&tids[0][i], NULL, test_put_data, (void *)&args[i]);
        pthread_create(&tids[1][i], NULL, test_get_data, (void *)&args[i]);
        pthread_create(&tids[2][i], NULL, test_invalidate_data, (void *)&args[i]);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void sequential_put(){
    int i, j, args[THREADS_PER_CALL];
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i], NULL, test_put_data, (void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_join(tids[0][i], NULL);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        args[i] = i;
        pthread_create(&tids[1][i],NULL,test_get_data,(void *)&args[i]);
        pthread_create(&tids[2][i],NULL,test_invalidate_data,(void *)&args[i]);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 1; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void sequential(){
    int i, j;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i], NULL, test_put_data, (void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[1][i], NULL, test_get_data, (void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[2][i], NULL, test_invalidate_data, (void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

int main(int argc, char *argv[]){

    if (check_input(argc, argv)) return -1;

    printf("Choose a test:\n"
           "\t[1] Sequential calls: n x put, n x get, n x invalidate\n"
           "\t[2] Sequential put: n x put, n x (get, invalidate)\n"
           "\t[3] Non-sequential: n x (put, get, invalidate)\n"
           "\t[other] Exit\n");

    pthread_barrier_init(&barrier, NULL, NUM_SYSCALLS*THREADS_PER_CALL);

    switch(getint()){
        case 1:
            sequential();
            break;
        case 2:
            sequential_put();
            break;
        case 3:
            non_sequential();
            break;
        default:
            break;
    }

    pthread_barrier_destroy(&barrier);

    return 0;
}