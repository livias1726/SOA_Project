#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	        /* Needed for KERN_INFO */
#include <linux/version.h>          /* Retrieve and format the current Linux kernel version */
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include "lib/include/scth.h"

#include "include/config.h"
#include "include/aos_fs.h"
#include "include/utils.h"

unsigned long the_syscall_table = 0x0;
module_param(the_syscall_table, ulong, 0660);

unsigned long the_ni_syscall;

unsigned long new_sys_call_array[] = {0x0, 0x0, 0x0};   //please set to sys_vtpmo at startup
#define HACKED_ENTRIES (int)(sizeof(new_sys_call_array)/sizeof(unsigned long))
int restore[HACKED_ENTRIES] = {[0 ... (HACKED_ENTRIES-1)] -1};

extern aos_fs_info_t *info;

/**
 * Put into one free block of the block-device 'size' bytes of the user-space data identified by the 'source' pointer.
 * This operation must be executed all or nothing.
 * When putting data, the operation of reporting data on the device can be either executed by the page-cache write back
 * daemon of the Linux kernel or immediately (in a synchronous manner) depending on a compile-time choice.
 * @return offset of the device (the block index) where data have been put;
 *         ENOMEM, if there is currently no room available on the device.
 * */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
#else
asmlinkage int sys_put_data(char * source, size_t size){
#endif
    struct aos_super_block aos_sb;
    int nblocks, avb_size, fail;
    uint64_t block_index, old_last;

    /* Check if device is mounted */
    check_mount;

    /* Signal device usage */
    __sync_fetch_and_add(&info->counter, 1);

    /* Check input parameter */
    aos_sb = info->sb;
    avb_size = aos_sb.data_block_size;
    if (size+1 >= avb_size) {
        fail = -EINVAL;
        goto failure_1;
    }

    /* Read bitmap to find a free block. Test and set: if a concurrent PUT retrieved the same block index, only
     * the first one to set the bit will be able to use it. The other one will try to find a new free block */
    nblocks = aos_sb.partition_size;
    do {
        block_index = find_first_zero_bit(info->free_blocks, nblocks);
        if(block_index == nblocks) { // no free block was found
            fail = -ENOMEM;
            goto failure_1;
        }
    } while (test_and_set_bit(block_index, info->free_blocks));

    DEBUG { printk(KERN_DEBUG "%s: [put_data() - %d] Started on block %llu\n", MODNAME, current->pid, block_index); }

    /* Signal a pending PUT on selected block */
    set_bit(block_index, info->put_map);

    /* Update last */
    old_last = __atomic_exchange_n(&info->last, block_index, __ATOMIC_SEQ_CST);
    DEBUG { printk(KERN_DEBUG "%s: [put_data() - %d] Atomically swapped 'last' from %llu to %llu. \n",
                   MODNAME, current->pid, old_last, block_index); }

    fail = put_new_block(block_index, source, size, old_last);
    if (fail < 0) goto failure_2;

    /* Signal completion of PUT operation on given block */
    wake_on_bit(info->put_map, block_index) //clear_bit(block_index, info->put_map);

    /* Release resources */
    __sync_fetch_and_sub(&info->counter, 1);
    wake_up_interruptible(&wq);

    AUDIT { printk(KERN_INFO "%s: [put_data() - %d] Put %d bytes in block %llu\n", MODNAME, current->pid, fail, block_index); }
    return block_index;

    failure_2:
        __sync_val_compare_and_swap(&info->last, block_index, old_last); // reset 'last' (if no thread has changed it)
        clear_bit(block_index, info->free_blocks);
        wake_on_bit(info->put_map, block_index) //clear_bit(block_index, info->put_map);
    failure_1:
        __sync_fetch_and_sub(&info->counter, 1);
        wake_up_interruptible(&wq);

        AUDIT { printk(KERN_INFO "%s: [put_data() - %d] Put failed on error %d\n", MODNAME, current->pid, fail); }
        return fail;
}

/**
 * Read up to 'size' bytes from the block at a given offset, if it currently keeps data.
 * @return number of bytes loaded into the destination area (zero, if no data is currently kept by the device block);
 *         ENODATA, if no data is currently valid and associated with the offset parameter.
 * */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(3, _get_data, uint64_t, offset, char *, destination, size_t, size){
#else
asmlinkage int sys_get_data(uint64_t offset, char * destination, size_t size){
#endif
    struct aos_super_block aos_sb;
    struct aos_data_block data_block;
    int loaded_bytes, len, fail;
    char * msg;
    size_t ret;

    /* Check if device is mounted */
    check_mount;

    /* Signal device usage */
    __sync_fetch_and_add(&info->counter, 1);

    /* Check input parameters */
    aos_sb = info->sb;
    if (offset < 2 || offset >= aos_sb.partition_size || size < 0 || size > aos_sb.data_block_size) {
        fail = -EINVAL;
        goto failure;
    }

    DEBUG { printk(KERN_DEBUG "%s: [get_data() - %d] Started on block %llu\n", MODNAME, current->pid, offset); }

    /* Read given block */
    fail = cpy_blk(info->vfs_sb, &info->block_locks[offset], offset, aos_sb.block_size, &data_block);
    if (fail < 0) goto failure;

    /* Check data validity */
    if (!data_block.metadata.is_valid) {
        fail = -ENODATA;
        goto failure;
    }

    msg = data_block.data.msg;

    /* Check message length */
    len = strlen(msg);
    if (len < size) size = len;

    /* Try to read 'size' bytes of data starting from 'offset' into 'destination' */
    ret = (len == 0) ? 0 : copy_to_user(destination, msg, size);
    loaded_bytes = size - ret;

    __sync_fetch_and_sub(&info->counter, 1);
    wake_up_interruptible(&wq);

    AUDIT { printk(KERN_INFO "%s: [get_data() - %d] Read %d bytes in block %llu\n",
                   MODNAME, current->pid, loaded_bytes, offset); }
    return loaded_bytes;

failure:
    __sync_fetch_and_sub(&info->counter, 1);
    wake_up_interruptible(&wq);

    AUDIT { printk(KERN_INFO "%s: [get_data() - %d] Get on block %llu failed with error %d\n",
                   MODNAME, current->pid, offset, fail); }

    return fail;
}

/**
 * Invalidate data in a block at a given offset. Data should logically disappear from the device.
 * @return ENODATA error if no data is currently valid and associated with the offset parameter
 * */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, uint32_t, offset){
#else
asmlinkage int sys_invalidate_data(uint32_t offset){
#endif
    int fail, nblocks;

    /* Check if device is mounted */
    check_mount;

    /* Signal device usage */
    __sync_fetch_and_add(&info->counter, 1);

    /* Check input parameters */
    nblocks = info->sb.partition_size;
    if (offset < 2 || offset >= nblocks) {
        fail = -EINVAL;
        goto failure_1;
    }

    DEBUG { printk(KERN_DEBUG "%s: [invalidate_data() - %d] Started on block %d\n", MODNAME, current->pid, offset); }

    /* Signal a pending INV on selected block. Test and set is used to atomically detect concurrent invalidations
     * on the same block and stop them all except for the first to set the flag. */
    if (test_and_set_bit(offset, info->inv_map)) {
        fail = -ENODATA;
        goto failure_1;
    }

    /* Check current pending PUT on the same block and free blocks bitmap:
     * if a PUT is pending on the block it means that the block has currently no valid data associated yet.
     * This falls into the case of ENODATA error. */
    if (test_bit(offset, info->put_map) || !test_bit(offset, info->free_blocks)) {
        fail = -ENODATA;
        goto failure_2;
    }

    fail = invalidate_block(offset);
    if (fail < 0) goto failure_2;

    /* Finalize the invalidation: set invalid block as free to write on and release the bit in INV_MAP */
    clear_bit(offset, info->free_blocks);
    clear_bit(offset, info->inv_map);

    /* Release resources */
    __sync_fetch_and_sub(&info->counter, 1);
    wake_up_interruptible(&wq);

    AUDIT { printk(KERN_INFO "%s: [invalidate_data() - %d] Invalidated block %d\n", MODNAME, current->pid, offset); }
    return 0;

    /* Failures behaviour */
    failure_2:
        clear_bit(offset, info->inv_map);
    failure_1:
        __sync_fetch_and_sub(&info->counter, 1);
        wake_up_interruptible(&wq);

        AUDIT { printk(KERN_INFO "%s: [invalidate_data() - %d] Invalidation of block %d failed with error %d.\n",
                       MODNAME, current->pid, offset, fail); }
        return fail;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
long sys_get_data = (unsigned long) __x64_sys_get_data;
long sys_put_data = (unsigned long) __x64_sys_put_data;
long sys_invalidate_data = (unsigned long) __x64_sys_invalidate_data;
#else
#endif

int register_syscalls()
{
    int i;
    int ret;

    AUDIT{
        printk(KERN_INFO "%s: installing new (%d) syscalls\n",MODNAME,HACKED_ENTRIES);
        printk(KERN_INFO "%s: received SCT address %px\n",MODNAME,(void*)the_syscall_table);
    }

    new_sys_call_array[0] = (unsigned long)sys_put_data;
    new_sys_call_array[1] = (unsigned long)sys_get_data;
    new_sys_call_array[2] = (unsigned long)sys_invalidate_data;

    ret = get_entries(restore,HACKED_ENTRIES,(unsigned long*)the_syscall_table,&the_ni_syscall);

    if (ret != HACKED_ENTRIES){
        printk(KERN_WARNING "%s: could not hack %d entries (just %d)\n",MODNAME,HACKED_ENTRIES,ret);
        return -1;
    }

    unprotect_memory();
    for(i=0;i<HACKED_ENTRIES;i++){
        ((unsigned long *)the_syscall_table)[restore[i]] = (unsigned long)new_sys_call_array[i];
    }
    protect_memory();

    printk(KERN_INFO "%s: all new syscalls correctly installed\n",MODNAME);

    return 0;
}

void unregister_syscalls()
{
    int i;

    printk(KERN_INFO "%s: removing new syscalls from SCT\n",MODNAME);

    unprotect_memory();
    for(i=0;i<HACKED_ENTRIES;i++){
        ((unsigned long *)the_syscall_table)[restore[i]] = the_ni_syscall;
    }
    protect_memory();

    printk(KERN_INFO "%s: SCT restored to its original content\n",MODNAME);
}