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

void all_calls(){
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

void get_and_inv(){
    int i, j;
    pthread_t tids[2][THREADS_PER_CALL];
    int ids_g[THREADS_PER_CALL];
    int ids_i[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_g[i] = i;
        ids_i[i] = i+THREADS_PER_CALL;

        pthread_create(&tids[0][i], NULL, multi_get_data, (void *)(ids_g+i));
        pthread_create(&tids[1][i], NULL, multi_invalidate_data, (void *)(ids_i+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < 2; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void put_and_get(){
    int i, j;
    pthread_t tids[2][THREADS_PER_CALL];
    int ids_p[THREADS_PER_CALL];
    int ids_g[THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_p[i] = i;
        ids_g[i] = i+THREADS_PER_CALL;

        pthread_create(&tids[0][i], NULL, multi_put_data, (void *)(ids_p+i));
        pthread_create(&tids[1][i], NULL, multi_get_data, (void *)(ids_g+i));
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < 2; ++j) {
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
           "\t[1] Only Put\n"
           "\t[2] Only Get\n"
           "\t[3] Only Invalidate\n"
           "\t[4] Put and Get\n"
           "\t[5] Get and Invalidate\n"
           "\t[6] Put and Invalidate\n"
           "\t[7] All\n"
           "\t[other] Exit\n");

    srand(time(NULL));

    switch(getint()){
        case 1:
            pthread_barrier_init(&barrier, NULL, THREADS_PER_CALL);
            single_call(multi_put_data);
            break;
        case 2:
            pthread_barrier_init(&barrier, NULL, THREADS_PER_CALL);
            single_call(multi_get_data);
            break;
        case 3:
            pthread_barrier_init(&barrier, NULL, THREADS_PER_CALL);
            single_call(multi_invalidate_data);
            break;
        case 4:
            pthread_barrier_init(&barrier, NULL, 2*THREADS_PER_CALL);
            put_and_get();
            break;
        case 5:
            pthread_barrier_init(&barrier, NULL, 2*THREADS_PER_CALL);
            get_and_inv();
            break;
        case 6:
            pthread_barrier_init(&barrier, NULL, 2*THREADS_PER_CALL);
            only_writers();
            break;
        case 7:
            pthread_barrier_init(&barrier, NULL, NUM_SYSCALLS*THREADS_PER_CALL);
            all_calls();
            break;
        default:
            break;
    }

    pthread_barrier_wait(&barrier);
    pthread_barrier_destroy(&barrier);

    return 0;
}
