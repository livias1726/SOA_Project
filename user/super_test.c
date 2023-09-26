#include "user.h"

int main(){
    int i, j, z = THREADS_PER_CALL*2;
    pthread_t tids[4][THREADS_PER_CALL];
    int ids_p[THREADS_PER_CALL];
    int ids_g[THREADS_PER_CALL];
    int ids_i[THREADS_PER_CALL];

    printf("Starting super test. Concurrent execution of 'n':\n"
           "System calls (PUT, INV, GET)\n"
           "Device operations (open, read, close)\n"
           /*"Device management (unmount).\n"*/);

    pthread_barrier_init(&barrier, NULL, THREADS_PER_CALL);
    srand(time(NULL));

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ids_p[i] = i;
        ids_g[i] = i+THREADS_PER_CALL;
        ids_i[i] = i+z;

        pthread_create(&tids[0][i], NULL, test_put_data, (void *)(ids_p+i));
        pthread_create(&tids[1][i], NULL, test_get_data, (void *)(ids_g+i));
        pthread_create(&tids[2][i], NULL, test_invalidate_data, (void *)(ids_i+i));
        pthread_create(&tids[3][i], NULL, multi_orc, NULL);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < 4; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }

    pthread_barrier_destroy(&barrier);

    return 0;
}
