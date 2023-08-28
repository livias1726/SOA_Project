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
    uint64_t *free_map;
    uint64_t nblocks;
    int i, j, fail, block_index = -1;
    size_t ret;

    // check if device is mounted
    if (!info->is_mounted) {
        fail = -ENODEV;
        goto dev_fail;
    }

    __sync_fetch_and_add(&info->count, 1);

    // check legal size
    if (size >= sizeof(struct aos_data_block)) {
        fail = -EINVAL;
        goto size_fail;
    }

    // take reader lock on bitmap
    read_lock(&info->fb_lock);
    free_map = info->free_blocks;
    nblocks = info->sb.partition_size;

    // find a free block
    for (i = 0; i < nblocks; i+=64) { // scan 64 bit at a time

        if ((*free_map & FULL_MAP_ENTRY) == FULL_MAP_ENTRY) {
            AUDIT{ printk(KERN_INFO "%s: %d (%p) full\n", MODNAME, i, free_map); }
            free_map++;
            continue;
        }

        // found block with a bit unset
        read_unlock(&info->fb_lock);
        write_lock(&info->fb_lock);
        for (j = 0; j < 64; ++j) {
            if (!(TEST_BIT(free_map, j))) {
                AUDIT{ printk(KERN_INFO "%s: %d, %d (%p) free\n", MODNAME, i, j, free_map); }
                block_index = i + j;
                break;
            }
        }
        break;
    }

    // no free block was found
    if(block_index == -1) {
        fail = -ENOMEM;
        read_unlock(&info->fb_lock);
        goto unavail_fail;
    }

    SET_BIT(info->free_blocks, block_index);

    // alloc block area to retrieve message from user
    data_block = kmalloc(sizeof(struct aos_data_block), GFP_KERNEL);
    if (!data_block) {
        fail = -ENOMEM;
        goto block_fail;
    }

    // retrieve message from user
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;

    // take writer lock on data block
    write_seqlock(&info->block_locks[block_index]);

    // get data block in page cache buffer
    bh = sb_bread(info->vfs_sb, block_index);
    if(!bh) {
        fail = -EIO;
        goto buff_fail;
    }

    write_unlock(&info->fb_lock);

    memcpy(bh->b_data, data_block, sizeof(*data_block));
    mark_buffer_dirty(bh);
#ifdef WB
    AUDIT { printk(KERN_INFO "%s: [put_data()] forcing synchronization on page cache\n", MODNAME); }
    sync_dirty_buffer(bh); // immediate synchronous write on the device
#endif

    // release resources
    brelse(bh);
    write_sequnlock(&info->block_locks[block_index]);
    __sync_fetch_and_sub(&info->count, 1);
    kfree(data_block);

    AUDIT { printk(KERN_INFO "%s: [put_data()] system call was successful - written %lu bytes in block %d\n",
                   MODNAME, size, block_index); }

    return block_index;

    buff_fail:
        kfree(data_block);
        write_sequnlock(&info->block_locks[block_index]);
    block_fail:
        CLEAR_BIT(info->free_blocks, block_index);
        write_unlock(&info->fb_lock);
    unavail_fail:
    size_fail:
        __sync_fetch_and_sub(&info->count, 1);
    dev_fail:
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

    // check if device is mounted
    if (!info->is_mounted) return -ENODEV;

    __sync_fetch_and_add(&info->count, 1);

    // check parameters
    aos_sb = info->sb;
    if (offset < 2 || offset > aos_sb.partition_size || size < 0 || size > aos_sb.block_size) {
        fail = -EINVAL;
        goto failure;
    }

    do {
        seq = read_seqbegin(&info->block_locks[offset]);
        // get data block in page cache buffer
        bh = sb_bread(info->vfs_sb, offset);
        if(!bh) {
            fail = -EIO;
            goto failure;
        }
        memcpy(&data_block, bh->b_data, sizeof(struct aos_data_block));
        brelse(bh);
    } while (read_seqretry(&info->block_locks[offset], seq));

    // check data validity
    if (!data_block.metadata.is_valid) {
        fail = -ENODATA;
        goto failure;
    }

    msg = data_block.data.msg;
    len = strlen(msg);
    if (len == 0) { // no available data
        fail = 0;
        goto failure;
    } else if (len < size) {
        size = len;
    }

    // try to read 'size' bytes of data starting from 'offset' into 'destination'
    ret = copy_to_user(destination, msg, size);
    loaded_bytes = size - ret;

    AUDIT { printk(KERN_INFO "%s: [get_data()] system call was successful - read %d bytes in block %llu\n",
                   MODNAME, loaded_bytes, offset); }

    fail = loaded_bytes;

    failure:
        __sync_fetch_and_sub(&info->count, 1);
        return fail;
}

/**
 * Invalidate data in a block at a given offset. Data should logically disappear from the device
 * @return ENODATA error if no data is currently valid and associated with the offset parameter
 * */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,17,0)
__SYSCALL_DEFINEx(1, _invalidate_data, uint64_t, offset){
#else
asmlinkage int sys_invalidate_data(uint64_t offset){
#endif
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    unsigned int seq;

    // check if device is mounted
    if (!info->is_mounted) return -ENODEV;

    __sync_fetch_and_add(&info->count, 1);

    // check legal operation
    if (offset < 2 || offset > info->sb.partition_size) {
        __sync_fetch_and_sub(&info->count, 1);
        return -EINVAL;
    }

    do {
        seq = read_seqbegin(&info->block_locks[offset]);
        // get data block in page cache buffer
        bh = sb_bread(info->vfs_sb, offset);
        if(!bh) {
            __sync_fetch_and_sub(&info->count, 1);
            return -EIO;
        }
        data_block = (struct aos_data_block*)bh->b_data;

        if (!data_block->metadata.is_valid) { // the block does not contain valid data
            brelse(bh);
            __sync_fetch_and_sub(&info->count, 1);
            return -ENODATA;
        }
    } while (read_seqretry(&info->block_locks[offset], seq));

    // set invalid block as free
    write_lock(&info->fb_lock);
    CLEAR_BIT(info->free_blocks, offset);
    write_unlock(&info->fb_lock);

    // invalidate data
    write_seqlock(&info->block_locks[offset]);
    data_block->metadata.is_valid = 0; // todo: check if invalid bit is set correctly
    mark_buffer_dirty(bh);
    brelse(bh);
    write_sequnlock(&info->block_locks[offset]);

    __sync_fetch_and_sub(&info->count, 1);

    AUDIT { printk(KERN_INFO "%s: [invalidate_data()] system call was successful - invalidated block %llu\n",
                   MODNAME, offset); }

    return 0;
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