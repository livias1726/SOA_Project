#ifndef SOA_PROJECT_CONFIG_H
#define SOA_PROJECT_CONFIG_H

#include <linux/moduleparam.h>

#define MODNAME "AOS"
#define AUDIT if(1)
#define DEBUG if(0)
#define WB if(0)
#define JIFFIES 100
#define SYSCALL_TRIALS 10
#define PUT_BIT 0

//MODULE_LICENSE("GPL");

int register_syscalls(void);
void unregister_syscalls(void);

extern uint64_t is_mounted;

#endif //SOA_PROJECT_CONFIG_H
