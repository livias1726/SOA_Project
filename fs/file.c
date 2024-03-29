#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/version.h>

#include "../include/aos_fs.h"
#include "../include/utils.h"

/**
 * The device driver should support file system operations allowing the access to the currently saved data:
 *  - 'open' for opening the device as a simple stream of bytes
 *  - 'release' for closing the file associated with the device
 *  - 'read' to access the device file content, according to the order of the delivery of data.
 * When the device is not mounted, the above file operations should simply return with error.
 */

extern aos_fs_info_t *info;

/*
 * Opens the device
 * */
int aos_open(struct inode *inode, struct file *filp){

    /* Check if device is mounted */
    check_mount;

    /* Signal device usage */
    __sync_fetch_and_add(&info->counter, 1);

    printk(KERN_INFO "%s: device file successfully opened by thread %d\n", MODNAME, current->pid);

    return 0;
}

/*
 * Releases the file object.
 * */
int aos_release(struct inode *inode, struct file *filp){

    filp->f_pos = 0;

    // atomic sub to usage counter
    __sync_fetch_and_sub(&(info->counter), 1);
    wake_up_interruptible(&wq);

    printk(KERN_INFO "%s: device file closed by thread %d\n",MODNAME, current->pid);

    return 0;
}

/*
 * Reads 'count' bytes from the device starting from the oldest message; the value *f_pos (which usually corresponds to
 * the file pointer) is ignored.
 * The content must be delivered chronologically and the operation should only return data related to messages
 * not invalidated before the access in read mode to the corresponding block of the device in an I/O session.
 * */
ssize_t aos_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {

    struct aos_super_block aos_sb;
    struct aos_data_block data_block;
    int len, ret, data_block_size, bytes_read;
    bool is_last;
    char *msg, *block_msg;
    loff_t b_idx, offset, nblocks, next;

    /* Check device state validity: if 'last' is 1, the device is empty */
    if (info->first == 0 || info->last == 1) return -ENODATA;

    /* Retrieve device info */
    aos_sb = info->sb;
    nblocks = aos_sb.partition_size;
    data_block_size = aos_sb.data_block_size;

    /* If the offset was set at NBLOCKS, then EOF is reached */
    if ((*f_pos >> 32) == nblocks) return 0;

    /* Check parameter validity */
    if (!count) return 0;
    if (!buf) return -EINVAL;

    /* Allocate memory */
    if (count > MAX_READ) { count = MAX_READ; }
    msg = kzalloc(count, GFP_KERNEL);
    if(!msg) return -ENOMEM;

    /* Parse file pointer */
    b_idx = (*f_pos == 0) ? info->first : (*f_pos >> 32);   // retrieve last block accessed by the current thread (high 32 bits)
    offset = *f_pos & 0x00000000ffffffff; // retrieve last byte accessed by the current thread (low 32 bits)

    AUDIT { printk(KERN_INFO "%s: read operation called by thread %d - fp is (%lld, %lld)\n",
           MODNAME, current->pid, b_idx, offset); }

    bytes_read = 0;
    while((bytes_read < count) && (!is_last)){
        is_last = (b_idx == info->last);

        /* Read data block into a local variable */
        ret = cpy_blk(info->vfs_sb, &info->block_locks[b_idx], b_idx, data_block_size, &data_block);
        if (ret < 0) {
            kfree(msg);
            return ret;
        }

        next = data_block.metadata.next;

        /* Check data validity: invalidation could happen while reading the block.
         * This ensures that a writing on the block is always detected, even if the read is already executing. */
        if (!data_block.metadata.is_valid) {
            if (next == 0) break;
            b_idx = next;
            offset = 0; // reset intra-block offset
            continue;
        }

        /* Use the file pointer offset to start reading from given position in the file */
        block_msg = (offset != 0) ? data_block.data.msg + offset : data_block.data.msg;
        len = strlen(block_msg);

        AUDIT { printk(KERN_DEBUG "%s: read operation accessed block %lld of the device\n", MODNAME, b_idx); }

        if ((bytes_read + len) > count) { // last block to read
            len = count - bytes_read;
            memcpy(msg + bytes_read, block_msg, len);
            bytes_read += len;
            offset += len;

            break;
        }

        memcpy(msg + bytes_read, block_msg, len);
        bytes_read += len;
        memcpy(msg + bytes_read, "\n", 1);
        bytes_read += 1;

        if (next == 0) break;
        b_idx = next;
        offset = 0; // reset intra-block offset
    }

    ret = (bytes_read > 0) ? copy_to_user(buf, msg, bytes_read) : 0;
    kfree(msg);

    // set high 32 bits of f_pos to the current index i and low 32 bits of f_pos to the new offset count
    *f_pos = (is_last) ? (nblocks << 32) : (b_idx << 32) | offset;

    AUDIT { printk(KERN_INFO "%s: read operation by thread %d completed\n", MODNAME, current->pid); }

    return (bytes_read - ret);
}

/*
 * Searches a directory for an inode corresponding to the filename included in a dentry object.
 * */
static struct dentry *aos_lookup(struct inode *parent_inode, struct dentry *dentry, unsigned int flags){
    struct aos_inode *aos_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *inode = NULL;

    printk(KERN_INFO "%s: running the lookup inode-function for name %s", MODNAME, dentry->d_name.name);

    if (strcmp(dentry->d_name.name, DEVICE_NAME)) return NULL;

    // get a locked inode from the cache
    inode = iget_locked(sb, INODE_BLOCK_IDX);
    if (!inode) return ERR_PTR(-ENOMEM);

    // already cached inode - simply return successfully
    if(!(inode->i_state & I_NEW)) return dentry;

    // new VFS inode for a regular file
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,12,0)
    inode_init_owner(&init_user_ns, inode, NULL, S_IFREG);
#else
    inode_init_owner(inode, NULL, S_IFREG);
#endif

    inode->i_mode = (S_IFREG | S_IRWXU | S_IROTH | S_IXOTH);
    inode->i_fop = &aos_file_ops;
    inode->i_op = &aos_inode_ops;

    // just one link for this file
    set_nlink(inode, 1);

    // retrieve the file size via the FS specific inode, putting it into the generic inode
    bh = (struct buffer_head *)sb_bread(sb, INODE_BLOCK_IDX);
    if(!bh){
        iput(inode);
        return ERR_PTR(-EIO);
    }
    aos_inode = (struct aos_inode*)bh->b_data;
    inode->i_size = aos_inode->file_size;
    brelse(bh);

    d_add(dentry, inode);
    dget(dentry);

    //unlock the inode to make it usable
    unlock_new_inode(inode);

    return dentry;
}

const struct inode_operations aos_inode_ops = {
    .lookup = aos_lookup
};

const struct file_operations aos_file_ops = {
    .owner = THIS_MODULE,
    .open = aos_open,
    .release = aos_release,
    .read = aos_read
};

