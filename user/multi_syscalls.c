#include "user.h"

int put;
int get;
int inv;

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

void* test_put_data(void *arg){
    int ret, tid = *(int*)arg, size = strlen(example_txt);
    char *msg = malloc(size+1);
    memcpy(msg, example_txt, size);
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

    char msg[DEFAULT_SIZE];

    ret = syscall(get, tid+2, msg, DEFAULT_SIZE);
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

int check_input(int argc, char *argv[]){
    if (argc < 4) {
        printf("Usage: ./user <PUT code> <GET code> <INVALIDATE code>");
        return -1;
    }

    int ret;

    put = atoi(argv[1]);
    get = atoi(argv[2]);
    inv = atoi(argv[3]);

    // test for syscalls existence with invalid params to return a known error
    ret = syscall(put, NULL, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to PUT returned with error. System call not installed.\n");

    ret = syscall(get, -1, NULL, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to GET returned with error. System call not installed.\n");

    ret = syscall(inv, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to INVALIDATE returned with error. System call not installed.\n");

    return 0;
}

void non_sequential(){
    int i, j;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i], NULL, test_put_data, (void *)&i);
        pthread_create(&tids[1][i], NULL, test_get_data, (void *)&i);
        pthread_create(&tids[2][i], NULL, test_invalidate_data, (void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }

}

void sequential_put(){
    int i, j;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i], NULL, test_put_data, (void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_join(tids[0][i], NULL);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[1][i],NULL,test_get_data,(void *)&i);
        pthread_create(&tids[2][i],NULL,test_invalidate_data,(void *)&i);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < 2; ++j) {
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

    int i;
    char c;

    if (check_input(argc, argv)) return -1;

    printf("Choose a test:\n"
           "\t[1] Sequential calls: n x put, n x get, n x invalidate\n"
           "\t[2] Sequential put: n x put, n x (get, invalidate)\n"
           "\t[3] Non-sequential: n x (put, get, invalidate)\n"
           "\t[other] Exit\n");

    pthread_barrier_init(&barrier, NULL, NUM_SYSCALLS*THREADS_PER_CALL);
    c = getc(stdin);
    i = atoi(&c);

    switch(i){
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