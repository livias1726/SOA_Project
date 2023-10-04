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
#define THREADS_PER_CALL 10
#define MAX_IN 5

#define LOREM "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
#define SIZE_LOREM 445
#define EMERALD "Verum, sine mendacio, certum et verissimum : quod est inferius est sicut quod est superius; et quod est superius est sicut quod est inferius, ad perpetranda miracula rei unius. Et sicut omnes res fuerunt ab uno, mediatione unius, sic omnes res natae fuerunt ab hac una re, adaptatione. Pater ejus est Sol, mater ejus Luna; portavit illud Ventus in ventre suo; nutrix ejus Terra est. Pater omnis telesdi est hic. Vis ejus integra est si versa fuerit in terram. Separabis terram ab igne, subtile a spisso, suaviter, cum magno ingenio. Ascendit a terra in caelum, iterumque descendit in terram, et recipit vim superiorum et inferiorum. Sic habebis gloriam totius mundi. Ideo fugiet a te omnis obscuritas. Haec est totius fortitudinis fortitudo fortis; quia vincet omnem rem subtilem, omnemque solidam penetrabit. Sic mundus creatus est. Hinc erunt adaptationes mirabiles, quarum modus est hic. Itaque vocatus sum Hermes Trismegistus, habens tres partes philosophiae totius mundi. Completum est quod dixi de operatione Solis."
#define SIZE_EMERALD 1021
#define LUCIFER "Lo ’mperador del doloroso regno da mezzo ’l petto uscia fuor de la ghiaccia; e più con un gigante io mi convegno, che i giganti non fan con le sue braccia: vedi oggimai quant’esser dee quel tutto ch’a così fatta parte si confaccia. S’el fu sì bel com’elli è ora brutto, e contra ’l suo fattore alzò le ciglia, ben dee da lui procedere ogne lutto. Oh quanto parve a me gran maraviglia quand’io vidi tre facce a la sua testa! L’una dinanzi, e quella era vermiglia; l’altr’eran due, che s’aggiugnieno a questa sovresso ’l mezzo di ciascuna spalla, e sé giugnieno al loco de la cresta: e la destra parea tra bianca e gialla; la sinistra a vedere era tal, quali vegnon di là onde ’l Nilo s’avvalla. Sotto ciascuna uscivan due grand’ali, quanto si convenia a tanto uccello: vele di mar non vid’io mai cotali. Non avean penne, ma di vispistrello era lor modo; e quelle svolazzava, sì che tre venti si movean da ello: quindi Cocito tutto s’aggelava. Con sei occhi piangëa, e per tre menti gocciava ’l pianto e sanguinosa bava. Da ogne bocca dirompea co’ denti un peccatore, a guisa di maciulla, sì che tre ne facea così dolenti. A quel dinanzi il mordere era nulla verso ’l graffiar, che talvolta la schiena rimanea de la pelle tutta brulla. \"Quell’anima là sù c’ ha maggior pena\", disse ’l maestro, \"è Giuda Scarïotto, che ’l capo ha dentro e fuor le gambe mena. De li altri due c’ hanno il capo di sotto, quel che pende dal nero ceffo è Bruto: vedi come si storce, e non fa motto!; e l’altro è Cassio, che par sì membruto. Ma la notte risurge, e oramai è da partir, ché tutto avem veduto."
#define SIZE_LUCIFER 1657

extern int put;
extern int get;
extern int inv;

int check_input(int argc, char **argv);
int getint();
void check_error(int tid, char* call);

// single thread
void test_put_data();
void test_get_data();
void test_invalidate_data();

// multi thread
void* multi_put_data(void *arg);
void* multi_get_data(void *arg);
void* multi_invalidate_data(void *arg);

// device
void orc();
void orc_fp();
void* multi_orc();

#endif //SOA_PROJECT_USER_H
