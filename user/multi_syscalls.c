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

int prepare_arg_structs(put_args_t *pa, get_arg_t *ga, inv_args_t *ia){
    pa = malloc(sizeof(put_args_t));
    if (!pa) return -1;

    ga = malloc(sizeof(get_arg_t));
    if (!ga) {
        free(pa);
        return -1;
    }

    ia = malloc(sizeof(inv_args_t));
    if(!ia){
        free(pa);
        free(ga);
        return -1;
    }

    return 0;
}

void* test_put_data(void *arg){
    int ret;
    put_args_t *pa = (put_args_t*)arg;

    pa->size = strlen(example_txt);
    pa->msg = malloc((pa->size)+1);
    memcpy(pa->msg, example_txt, pa->size);
    pa->size = strlen(pa->msg);

    ret = syscall(pa->sysno, pa->msg, pa->size);
    if(ret < 0) {
        check_error();
        pthread_exit((void*)-1);
    } else {
        pthread_exit(0);
    }
}

void* test_get_data(void *arg) {
    int ret;
    get_arg_t *ga = (get_arg_t*)arg;

    ga->msg = malloc(DEFAULT_SIZE);
    if (!ga->msg) {
        perror("malloc failed.");
        pthread_exit((void*)-1);
    }

    ret = syscall(ga->sysno, ga->block, ga->msg, ga->size);
    if(ret < 0) {
        check_error();
        free(ga->msg);
        pthread_exit((void*)-1);
    } else {
        free(ga->msg);
        pthread_exit(0);
    }
}

void* test_invalidate_data(void *arg){
    int ret;
    inv_args_t *ia = (inv_args_t*)arg;

    ret = syscall(ia->sysno, ia->block);
    if(ret < 0) {
        check_error();
        pthread_exit((void*)-1);
    } else {
        pthread_exit(0);
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

    ret = syscall(*inv, -1);
    if(ret == -1 && errno == ENOSYS) printf("Test to INVALIDATE returned with error. System call not installed.\n");

    return 0;
}

void non_sequential(put_args_t *pa, get_arg_t *ga, inv_args_t *ia){
    int i, j, block_idx = 0;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i], NULL, test_put_data, pa);
        ga->block = block_idx;
        pthread_create(&tids[1][i], NULL, test_get_data, ga);
        ia->block = block_idx++;
        pthread_create(&tids[2][i], NULL, test_invalidate_data, ia);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }

}

void sequential_put(put_args_t *pa, get_arg_t *ga, inv_args_t *ia){
    int i, j, block_idx = 0;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i],NULL,test_put_data,pa);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_join(tids[0][i], NULL);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        ga->block = block_idx;
        pthread_create(&tids[1][i],NULL,test_get_data,ga);
        ia->block = block_idx++;
        pthread_create(&tids[2][i],NULL,test_invalidate_data,ia);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < 2; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

void sequential(put_args_t *pa, get_arg_t *ga, inv_args_t *ia){
    int i, j;
    pthread_t tids[NUM_SYSCALLS][THREADS_PER_CALL];

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[0][i],NULL,test_put_data,pa);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[1][i],NULL,test_get_data,ga);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        pthread_create(&tids[2][i],NULL,test_invalidate_data,ia);
    }

    for (i = 0; i < THREADS_PER_CALL; ++i) {
        for (j = 0; j < NUM_SYSCALLS; ++j) {
            pthread_join(tids[j][i], NULL);
        }
    }
}

int main(int argc, char *argv[]){

    int put, get, inv, i;
    char c;
    if (check_input(argc, argv, &put, &get, &inv)) return -1;

    put_args_t *pa = NULL;
    get_arg_t *ga = NULL;
    inv_args_t *ia = NULL;
    if (prepare_arg_structs(pa, ga, ia)) return -1;

    pa->sysno = put;
    ga->sysno = get;
    ia->sysno = inv;

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
            sequential(pa, ga, ia);
            break;
        case 2:
            sequential_put(pa, ga, ia);
            break;
        case 3:
            non_sequential(pa, ga, ia);
            break;
        default:
            break;
    }

    pthread_barrier_destroy(&barrier);
    return 0;
}