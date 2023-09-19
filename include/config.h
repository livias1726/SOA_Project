#ifndef SOA_PROJECT_CONFIG_H
#define SOA_PROJECT_CONFIG_H

#include <linux/moduleparam.h>

#define MODNAME "AOS"
#define AUDIT if(1)
#define WB if(0)

//MODULE_LICENSE("GPL");

int register_syscalls(void);
void unregister_syscalls(void);

#endif //SOA_PROJECT_CONFIG_H
