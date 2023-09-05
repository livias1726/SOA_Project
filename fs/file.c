#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/string.h>

#include "include/aos_fs.h"

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

    // check if the FS is mounted
    if (!info->is_mounted) {
        printk(KERN_WARNING "%s: [aos_open()] operation failed - fs not mounted.\n", MODNAME);
        return -ENODEV;
    }

    // atomic add to usage counter
    __sync_fetch_and_add(&(info->count),1);

    printk(KERN_INFO "%s: device file successfully opened by thread %d\n", MODNAME, current->pid);

    return 0;
}

/*
 * Releases the file object.
 * */
int aos_release(struct inode *inode, struct file *filp){

    // check if the FS is mounted
    if (!info->is_mounted) {
        printk(KERN_WARNING "%s: [aos_release()] operation failed - fs not mounted.\n", MODNAME);
        return -ENODEV;
    }

    // atomic sub to usage counter
    __sync_fetch_and_sub(&(info->count),1);

    printk(KERN_INFO "%s: device file closed by thread %d\n",MODNAME, current->pid);

    return 0;
}

/*
 * Reads 'count' bytes from the device starting from the oldest message; the value *f_pos (which usually corresponds to
 * the file pointer) is ignored.
 * todo The content must be delivered chronologically and the operation should only return data related to messages
 *  not invalidated before the access in read mode to the corresponding block of the device in an I/O session.
 * */
ssize_t aos_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {

    struct buffer_head *bh;
    struct aos_super_block aos_sb;
    struct aos_data_block data_block;
    int i, len, ret, nblocks, data_block_size;
    unsigned int seq;
    char *msg, *block_msg;
    loff_t offset;

    // check if device is mounted
    if (!info->is_mounted) return -ENODEV;

    printk(KERN_INFO "%s: read operation called by thread %d\n", MODNAME, current->pid);

    aos_sb = info->sb;
    nblocks = aos_sb.partition_size;
    data_block_size = aos_sb.data_block_size;

    msg = kzalloc(count, GFP_KERNEL);
    if(!msg) return -ENOMEM;

    offset = 0;
    for (i = 2; i < nblocks; ++i) {

        // read data block into local variable
        do {
            seq = read_seqbegin(&info->block_locks[i]);
            // get data block in page cache buffer
            bh = sb_bread(info->vfs_sb, i);
            if(!bh) {
                kfree(msg);
                return -EIO;
            }
            memcpy(&data_block, bh->b_data, data_block_size);
            brelse(bh);
        } while (read_seqretry(&info->block_locks[i], seq));

        // check data validity
        if (!data_block.metadata.is_valid) continue;

        // check data availability
        block_msg = data_block.data.msg;
        len = strlen(block_msg);
        if (len == 0) continue;

        AUDIT { printk(KERN_INFO "%s: read operation must access block %d of the device\n", MODNAME, i); }

        if ((offset + len) > count) { // last block to read
            len = count - offset;
            if (len > 0) {
                memcpy(msg + offset, block_msg, len);
                offset += len;
            }
            break;
        } else {
            memcpy(msg + offset, block_msg, len);
            offset += len;
            memcpy(msg + offset, "\n", 1);
            offset += 1;
        }
    }

    ret = (offset > 0) ? copy_to_user(buf, msg, offset) : 0;
    kfree(msg);

    AUDIT { printk(KERN_INFO "%s: read operation by thread %d completed\n", MODNAME, current->pid); }

    return (offset - ret);
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
    inode_init_owner(inode, NULL, S_IFREG);
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

