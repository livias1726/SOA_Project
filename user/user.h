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
#define NBLOCKS 10
#define DEVICE_SIZE (4096 * NBLOCKS)
#define NUM_SYSCALLS 3
#define THREADS_PER_CALL 100
#define MAX_IN 5

#define LOREM "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
#define SIZE_LOREM 445
#define EMERALD "Verum, sine mendacio, certum et verissimum : quod est inferius est sicut quod est superius; et quod est superius est sicut quod est inferius, ad perpetranda miracula rei unius. Et sicut omnes res fuerunt ab uno, mediatione unius, sic omnes res natae fuerunt ab hac una re, adaptatione. Pater ejus est Sol, mater ejus Luna; portavit illud Ventus in ventre suo; nutrix ejus Terra est. Pater omnis telesmi totius mundi est hic. Vis ejus integra est si versa fuerit in terram. Separabis terram ab igne, subtile a spisso, suaviter, cum magno ingenio. Ascendit a terra in caelum, iterumque descendit in terram, et recipit vim superiorum et inferiorum. Sic habebis gloriam totius mundi. Ideo fugiet a te omnis obscuritas. Haec est totius fortitudinis fortitudo fortis; quia vincet omnem rem subtilem, omnemque solidam penetrabit. Sic mundus creatus est. Hinc erunt adaptationes mirabiles, quarum modus est hic. Itaque vocatus sum Hermes Trismegistus, habens tres partes philosophiae totius mundi. Completum est quod dixi de operatione Solis."
#define SIZE_EMERALD 1034

pthread_barrier_t barrier;
int put;
int get;
int inv;

int check_input(int argc, char **argv);
int getint();
void check_error(int tid);

#endif //SOA_PROJECT_USER_H
