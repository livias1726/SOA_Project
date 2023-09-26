#include "user.h"

void only_writers(){
    int i, j;
    pthread_t tids[2][THREADS_PER_CALL];
    int ids_p[THREADS_PER_CALL];
    int ids_i[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_p[i] = i;
        ids_i[i] = i+THREADS_PER_CALL;

        pthread_create(&tids[0][i], NULL, multi_put_data, (void *)(ids_p+i));
        pthread_create(&tids[1][i], NULL, multi_invalidate_data, (void *)(ids_i+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < 2; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void non_sequential(){
    int i, j, z = THREADS_PER_CALL*2;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];
    int ids_p[THREADS_PER_CALL];
    int ids_g[THREADS_PER_CALL];
    int ids_i[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_p[i] = i;
        ids_g[i] = i+THREADS_PER_CALL;
        ids_i[i] = i+z;

        pthread_create(&tids[0][i], NULL, multi_put_data, (void *)(ids_p+i));
        pthread_create(&tids[1][i], NULL, multi_get_data, (void *)(ids_g+i));
        pthread_create(&tids[2][i], NULL, multi_invalidate_data, (void *)(ids_i+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void sequential_put(){
    int i, j, z;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];
    int ids_p[THREADS_PER_CALL];
    int ids_g[THREADS_PER_CALL];
    int ids_i[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_p[i] = i;
        pthread_create(&tids[0][i], NULL, multi_put_data, (void *)(ids_p+i));
    }

    z = THREADS_PER_CALL*2;
    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_g[i] = i+THREADS_PER_CALL;
        ids_i[i] = i+z;
        pthread_create(&tids[1][i], NULL, multi_get_data, (void *)(ids_g+i));
        pthread_create(&tids[2][i], NULL, multi_invalidate_data, (void *)(ids_i+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void sequential(){
    int i, j, z;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];
    int ids_p[THREADS_PER_CALL];
    int ids_g[THREADS_PER_CALL];
    int ids_i[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_p[i] = i;
        pthread_create(&tids[0][i], NULL, multi_put_data, (void *)(ids_p+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_g[i] = i+THREADS_PER_CALL;
        pthread_create(&tids[1][i], NULL, multi_get_data, (void *)(ids_g+i));
    }

    z = THREADS_PER_CALL*2;
    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_i[i] = i+z;
        pthread_create(&tids[2][i], NULL, multi_invalidate_data, (void *)(ids_i+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void single_call(void* func(void*)) {
    int i;
    pthread_t tids[THREADS_PER_CALL];
    int ids[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids[i] = i;
        pthread_create(&tids[i], NULL, func, (void *)(ids+i));
    }
    for (i = 0; i < THREADS_PER_CALL; ++i) pthread_join(tids[i], NULL);
}

int main(int argc, char *argv[]){

    if (check_input(argc, argv)) return -1;

    printf("Choose a test:\n"
           "\t[1] Only put\n"
           "\t[2] Only get\n"
           "\t[3] Only invalidate\n"
           "\t[4] Sequential calls: n x put, n x get, n x invalidate\n"
           "\t[5] Sequential put: n x put, n x (get, invalidate)\n"
           "\t[6] Non-sequential: n x (put, get, invalidate)\n"
           "\t[7] Only writers: n x (put, invalidate)\n"
           "\t[other] Exit\n");

    pthread_barrier_init(&barrier, NULL, NUM_SYSCALLS*THREADS_PER_CALL);

    srand(time(NULL));

    switch(getint()){
        case 1:
            single_call(multi_put_data);
            break;
        case 2:
            single_call(multi_get_data);
            break;
        case 3:
            single_call(multi_invalidate_data);
            break;
        case 4:
            sequential();
            break;
        case 5:
            sequential_put();
            break;
        case 6:
            non_sequential();
            break;
        case 7:
            only_writers();
            break;
        default:
            break;
    }

    pthread_barrier_destroy(&barrier);

    return 0;
}
