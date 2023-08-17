#ifndef SOA_PROJECT_USER_H
#define SOA_PROJECT_USER_H

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define DEVICE_PATH "../fs/mount/the-device"
#define DEVICE_SIZE (4096 * 10)
#define NUM_SYSCALLS 3
#define THREADS_PER_CALL 10
#define DEFAULT_SIZE 445

char example_txt[] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
pthread_barrier_t barrier;

#endif //SOA_PROJECT_USER_H
