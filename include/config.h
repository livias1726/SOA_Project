#ifndef SOA_PROJECT_CONFIG_H
#define SOA_PROJECT_CONFIG_H

#include <linux/moduleparam.h>

#define MODNAME "AOS"

// Output printing configuration
#define AUDIT if(1)
#define DEBUG if(1)

// Execution restrictions
#define WB if(0)                /* Synchronous PUT */
#define SEQ_INV            /* The invalidation has to be guaranteed */
//#define TIMEOUT_INV    /* The invalidation performs a few trials to overcome a possible deadlock */
//#define RELAXED_INV          /* The invalidation that detect a conflict with other invalidations aborts */

// Tunable parameters
#define JIFFIES 100
#define SYSCALL_TRIALS 10

//MODULE_LICENSE("GPL");

int register_syscalls(void);
void unregister_syscalls(void);

extern uint64_t is_mounted;

#endif //SOA_PROJECT_CONFIG_H
