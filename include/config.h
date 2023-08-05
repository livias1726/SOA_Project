#ifndef SOA_PROJECT_CONFIG_H
#define SOA_PROJECT_CONFIG_H

#include <linux/moduleparam.h>

#define MODNAME "AOS"
#define AUDIT if(1)

MODULE_LICENSE("GPL");

int register_syscalls();
void unregister_syscalls();

#endif //SOA_PROJECT_CONFIG_H
