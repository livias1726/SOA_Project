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
    aos_fs_info_t *aos_info;
    struct super_block *sb;
    struct buffer_head *bh;
    struct aos_super_block* aos_sb;
    uint64_t* free_map;
    uint64_t nblocks;
    int i, j, fail, block_index = -1;
    char * msg;
    size_t ret;

    aos_info = &fs_info;

    // check if device is mounted
    if (!aos_info->is_mounted) { // checkme: try to check if mount status can be detected from sb_bread
        fail = -ENODEV;
        goto put_failure;
    }

    // check legal size
    if (size < 0 || size >= sizeof(struct aos_data_block)) {
        fail = -EINVAL;
        goto put_failure;
    }

    // find a free block
    aos_sb = aos_info->sb;
    free_map = aos_info->free_blocks;
    nblocks = aos_sb->partition_size;
    for (i = 0; i < nblocks; i+=64) { // scan 64 bit at a time
        if ((*free_map & FULL_MAP_ENTRY) == FULL_MAP_ENTRY) {
            free_map++;
            continue;
        }

        // found bit block with a bit unset
        for (j = 0; j < 64; ++j) {
            if (TEST_BIT(free_map, j)) {
                block_index = i + j;
                break;
            }
        }
        break;
    }

    if(block_index == -1) {
        fail = -ENOMEM;
        goto put_failure;
    }

    SET_BIT(aos_info->free_blocks, block_index);

    // alloc area to retrieve message from user
    msg = kzalloc(size, GFP_KERNEL);
    if (!msg) {
        fail = -ENOMEM;
        goto put_failure;
    }
    ret = copy_from_user(msg, source, size);
    size -= ret;
    msg[size+1] = '\0';

    sb = aos_info->vfs_sb;

    // get data block in page cache buffer
    bh = sb_bread(sb, block_index);
    if(!bh) {
        fail = -EIO;
        goto put_failure;
    }

    // write data on the free block
    memcpy(bh->b_data, msg, size);
    mark_buffer_dirty(bh);
#ifdef WB
    sync_dirty_buffer(bh); // immediate synchronous write on the device
#endif
    brelse(bh);

    return block_index;

put_failure:
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

    /* warning:
     *      this operation is performed by a reader on a given data block:
     *      need to acquire a reader lock on the block data to avoid conflicts if another thread concurrently tries
     *      to invalidate data.
     * */

    aos_fs_info_t *aos_info;
    struct super_block *sb;
    struct buffer_head *bh;
    struct aos_super_block *aos_sb;
    struct aos_data_block *data_block = NULL;
    int loaded_bytes, len;
    char * msg;
    size_t ret;

    // check if device is mounted
    aos_info = &fs_info;
    if (!aos_info->is_mounted) return -ENODEV;

    // check parameters
    aos_sb = aos_info->sb;
    if (offset < 2 || offset > aos_sb->partition_size || size < 0 || size > aos_sb->block_size) return -EINVAL;

    // todo: signal the presence of a reader on the block

    // get data block in page cache buffer
    sb = aos_info->vfs_sb;
    bh = sb_bread(sb, offset);
    if(!bh) {
        // todo: release reader lock
        return -EIO;
    }
    memcpy(data_block, bh->b_data, sizeof(struct aos_data_block));
    brelse(bh);

    // check data validity
    if (!data_block->metadata.is_valid) {
        // todo: release reader lock
        return -ENODATA;
    }

    msg = data_block->data.msg;
    len = strlen(msg);
    if (len == 0) {
        // todo: release reader lock
        return 0; // no available data
    } else if (len < size) {
        size = len;
    }

    // try to read 'size' bytes of data starting from 'offset' into 'destination'
    ret = copy_to_user(destination, msg, size);
    loaded_bytes = size - ret;

    // todo: release reader lock
    return loaded_bytes;
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

    aos_fs_info_t *aos_info;
    struct super_block *sb;
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    int fail;

    aos_info = &fs_info;
    // check if device is mounted
    if (!aos_info->is_mounted) {
        fail = -ENODEV;
        goto inv_failure;
    }
    // check legal operation
    if (offset < 2 || offset > aos_info->sb->partition_size) {
        fail = -EINVAL;
        goto inv_failure;
    }

    // get data block in page cache buffer
    sb = aos_info->vfs_sb;
    bh = sb_bread(sb, offset);
    if(!bh) {
        fail = -EIO;
        goto inv_failure;
    }
    data_block = (struct aos_data_block*)bh->b_data;

    if (!data_block->metadata.is_valid) return -ENODATA; // no valid data

    // invalidate data
    data_block->metadata.is_valid = 0;

    // set invalid block as free
    CLEAR_BIT(aos_info->free_blocks, offset);

    return 0;

inv_failure:
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