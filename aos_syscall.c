#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>	        /* Needed for KERN_INFO */
#include <linux/version.h>          /* Retrieve and format the current Linux kernel version */
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/buffer_head.h>
#include "lib/include/scth.h"

#include "include/config.h"
#include "fs/include/aos_fs.h"

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
 * @return offset of the device (the block index) where data have been put;
 *         ENOMEM, if there is currently no room available on the device.
 * */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(2, _put_data, char *, source, size_t, size){
#else
asmlinkage int sys_put_data(char * source, size_t size){
#endif
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    struct aos_super_block aos_sb;
    int nblocks, block_index, avb_size, fail, from;
    size_t ret;

    /* Check if device is mounted */
    if (!info->is_mounted) return -ENODEV;

    /* Signal device usage */
    __sync_fetch_and_add(&info->count, 1);

    /* Check input parameter */
    aos_sb = info->sb;
    avb_size = aos_sb.data_block_size;
    if (size+1 >= avb_size) {
        fail = -EINVAL;
        goto failure;
    }

    /* Read bitmap to find a free block */
    nblocks = aos_sb.partition_size;
    do {
        from = (info->last_put.counter == nblocks-1) ? 2 : info->last_put.counter;
        block_index = find_next_zero_bit(info->free_blocks, nblocks, from);
        if(block_index == nblocks) { // no free block was found
            fail = -ENOMEM;
            goto failure;
        }
    /* ATOMICALLY test and set the bit of the free block: if a concurrent PUT retrieved the same block index, only
     * the first one to set the bit will be able to use it. The other one will try to find a new free block */
    } while (test_and_set_bit(block_index, info->free_blocks));

    /* Allocate block */
    data_block = kmalloc(aos_sb.block_size, GFP_KERNEL);
    if (!data_block) {
        fail = -ENOMEM;
        goto failure_2;
    }

    /* Retrieve message from user */
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;

    /* Write block on device */
    bh = sb_bread(info->vfs_sb, block_index);
    if(!bh) {
        kfree(data_block);
        fail = -EIO;
        goto failure_2;
    }
    write_seqlock(&info->block_locks[block_index]);
    memcpy(bh->b_data, data_block, sizeof(*data_block));
    write_sequnlock(&info->block_locks[block_index]);

    /* ATOMICALLY re-set the bit of the free block in case an invalidate was executed in the meantime
     * this is needed when the invalidate operation only looks for set bits in the free map,
     * without checking the valid metadata: this speeds up the invalidate operation but can interfere with
     * concurrent put operations */
    set_bit(block_index, info->free_blocks);

    /* Atomically sets the value of last_put to block_index */
    atomic64_set(&info->last_put, block_index);

    mark_buffer_dirty(bh);
    WB { sync_dirty_buffer(bh); } // immediate synchronous write on the device

    /* Release resources */
    brelse(bh);
    kfree(data_block);
    __sync_fetch_and_sub(&info->count, 1);

    AUDIT { printk(KERN_INFO "%s: [put_data()] successful - written %lu bytes in block %d\n", MODNAME, size, block_index); }

    return block_index;

    failure_2:
        clear_bit(block_index, info->free_blocks);
    failure:
        __sync_fetch_and_sub(&info->count, 1);

        AUDIT { printk(KERN_INFO "%s: [put_data()] failed on error %d\n", MODNAME, fail); }

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
    struct buffer_head *bh;
    struct aos_super_block aos_sb;
    struct aos_data_block data_block;
    int loaded_bytes, len, fail;
    unsigned int seq;
    char * msg;
    size_t ret;

    /* Check if device is mounted */
    if (!info->is_mounted) return -ENODEV;

    /* Signal device usage */
    __sync_fetch_and_add(&info->count, 1);

    /* Check input parameters */
    aos_sb = info->sb;
    if (offset < 2 || offset > aos_sb.partition_size || size < 0 || size > aos_sb.data_block_size) {
        fail = -EINVAL;
        goto failure;
    }

    /* Read given block */
    do {
        seq = read_seqbegin(&info->block_locks[offset]);
        bh = sb_bread(info->vfs_sb, offset);
        if(!bh) {
            fail = -EIO;
            goto failure;
        }
        // copy the data in another memory location to release the buffer head
        memcpy(&data_block, bh->b_data, aos_sb.block_size);
        brelse(bh);
    } while (read_seqretry(&info->block_locks[offset], seq));

    /* Check data validity: test with bit operations on free map is not atomic. This implementation ensure that
     * data block remains the same after a read was successful according to the seqlock */
    if (!data_block.metadata.is_valid) {
        fail = -ENODATA;
        goto failure;
    }

    msg = data_block.data.msg;

    /* Check message length */
    len = strlen(msg);
    if (len == 0) { // no available data
        fail = 0;
        goto failure;
    } else if (len < size) {
        size = len;
    }

    /* Try to read 'size' bytes of data starting from 'offset' into 'destination' */
    ret = copy_to_user(destination, msg, size);
    loaded_bytes = size - ret;

    __sync_fetch_and_sub(&info->count, 1);

    AUDIT { printk(KERN_INFO "%s: [get_data()] successful - read %d bytes in block %llu\n",
                   MODNAME, loaded_bytes, offset); }

    return loaded_bytes;

    failure:
        AUDIT { printk(KERN_INFO "%s: [get_data()] on block %llu failed with error %d\n", MODNAME, offset, fail); }
        __sync_fetch_and_sub(&info->count, 1);
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
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    unsigned int seq;
    int fail, nblocks, first;

    /* Check if device is mounted */
    if (!info->is_mounted) return -ENODEV;

    /* Signal device usage */
    __sync_fetch_and_add(&info->count, 1);

    /* Check input parameters */
    nblocks = info->sb.partition_size;
    if (offset < 2 || offset >= nblocks) {
        fail = -EINVAL;
        goto failure;
    }

    /* Read a block until no concurrent write is detected */
    do {
        seq = read_seqbegin(&info->block_locks[offset]);
        bh = sb_bread(info->vfs_sb, offset);
        if(!bh) {
            fail = -EIO;
            goto failure;
        }
        data_block = (struct aos_data_block*)bh->b_data;
    } while (read_seqretry(&info->block_locks[offset], seq));

    /* Check the presence of valid data in the block */
    if (!data_block->metadata.is_valid) {
        brelse(bh);
        fail = -ENODATA;
        goto failure;
    }

    /* Set invalid block as free to write on */
    clear_bit(offset, info->free_blocks);

    /* Change the block's metadata */
    write_seqlock(&info->block_locks[offset]);
    data_block->metadata.is_valid = 0;
    write_sequnlock(&info->block_locks[offset]);

    /* Update the index of first block to read chronologically */
    if(atomic64_read(&info->first_block) == offset) {
        first = find_next_bit(info->free_blocks, nblocks, offset);
        if(first == nblocks) { // no set bit after this
            if (offset != 2) {
                first = find_next_bit(info->free_blocks, offset, 2); // try for the previous bits
                if (first == offset) {
                    first = 2;
                    atomic64_set(&info->last_put, 1);
                }
            } else {
                first = 2;
                atomic64_set(&info->last_put, 1);
            }
        }
        atomic64_set(&info->first_block, first);
        AUDIT { printk(KERN_INFO "%s: [invalidate_data()] on block %d set first = %d.\n", MODNAME, offset, first); }
    }

    mark_buffer_dirty(bh);
    brelse(bh);
    __sync_fetch_and_sub(&info->count, 1);

    AUDIT { printk(KERN_INFO "%s: [invalidate_data()] successful - invalidated block %d\n", MODNAME, offset); }
    return 0;

    failure:
        AUDIT { printk(KERN_INFO "%s: [invalidate_data()] on block %d failed with error %d.\n", MODNAME, offset, fail); }
        __sync_fetch_and_sub(&info->count, 1);
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