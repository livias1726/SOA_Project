#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	        /* Needed for KERN_INFO */
#include <linux/fs.h>               /* File system */
#include <linux/version.h>          /* Retrieve and format the current Linux kernel version */
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>

#include "include/config.h"
#include "include/aos_fs.h"

static int __init init_driver(void)
{
    int err;

    printk(KERN_INFO "%s: initializing device driver\n",MODNAME);

    err = register_syscalls();
    if(err) goto fail_syscall;

    printk(KERN_INFO "%s: trying to register AOS file system\n",MODNAME);
    err = register_filesystem(&aos_fs_type);
    if (err) goto fail_fs;
    printk(KERN_INFO "%s: correctly registered AOS file system\n",MODNAME);

    return 0;

    fail_fs:
        printk(KERN_ALERT "%s: couldn't register AOS file system\n",MODNAME);
    fail_syscall:
        return err;
}

static void __exit exit_driver(void)
{
    int err;
    printk(KERN_INFO "%s: uninstalling device driver\n",MODNAME);

    err = unregister_filesystem(&aos_fs_type);

    if (err) printk(KERN_ALERT "%s: failed to unregister file system driver (error %d)", MODNAME, err);

    unregister_syscalls();
}

module_init(init_driver)
module_exit(exit_driver)






