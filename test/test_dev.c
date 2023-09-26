#include "user.h"

int main(){
    int i;
    pthread_t tids[THREADS_PER_CALL];

    printf("Choose a test on file system operations:\n"
           "\t[1] ST Open-Read-Close\n"
           "\t[2] ST Open-(Multi)Read-Close\n"
           "\t[3] MT Open-Read-Close\n"
           "\t[other] Exit\n");

    switch(getint()){
        case 1:
            orc();
            break;
        case 2:
            orc_fp();
            break;
        case 3:
            pthread_barrier_init(&barrier, NULL, THREADS_PER_CALL);

            for (i = 0; i < THREADS_PER_CALL; ++i) pthread_create(&tids[i], NULL, multi_orc, NULL);
            for (i = 0; i < THREADS_PER_CALL; ++i) pthread_join(tids[i], NULL);

            pthread_barrier_destroy(&barrier);
            break;
        default:
            break;
    }

    return 0;
}