#include <linux/module.h>
#include <linux/fs.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "include/aos_fs.h"

/**
 * The device driver should support file system operations allowing the access to the currently saved data:
 *  - 'open' for opening the device as a simple stream of bytes
 *  - 'release' for closing the file associated with the device
 *  - 'read' to access the device file content, according to the order of the delivery of data.
 * When the device is not mounted, the above file operations should simply return with error.
 */

/*
 * Opens a file by creating a new file object and linking it to the corresponding inode object
 * */
int aos_open(struct inode *inode, struct file *filp){
    aos_fs_info_t *info;
    struct aos_inode *aos_inode;

    info = inode->i_sb->s_fs_info;
    if (!info->is_mounted) return -ENODEV;

    /* check permission */
    if (filp->f_flags & FMODE_WRITE) // todo: do something

    atomic_inc(&scull_s_available);

    printk("%s: device file successfully opened by thread %d\n", MODNAME, current->pid);

    return 0;
}

/*
 * Releases the file object. Called when the last reference to an open file is closed—
 * that is, when the f_count field of the file object becomes 0.
 * */
int aos_release(struct inode *inode, struct file *filp){


    printk("%s: device file closed by thread %d\n",MODNAME, current->pid);

    return 0;
}

/*
 * Reads count bytes from a file starting at position *f_pos;
 * the value *f_pos (which usually corresponds to the file pointer) is then increased.
 *
 * A read operation should only return data related to messages not invalidated
 * before the access in read mode to the corresponding block of the device in an I/O session.
 *
 * // todo: check if device is mounted
 * */
ssize_t aos_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {

    struct buffer_head *bh = NULL;
    struct inode * the_inode = filp->f_inode;
    uint64_t file_size = the_inode->i_size;
    int ret;
    loff_t offset;
    int block_to_read;  //index of the block to be read from device

    aos_fs_info_t *info = the_inode->i_sb->s_fs_info;
    if(!info->is_mounted) return -ENODEV;

    printk("%s: read operation called with len %ld and offset %lld (the current file size is %lld)",
           MODNAME, count, *f_pos, file_size);

    // check boundaries
    if (*f_pos >= file_size) {
        return 0;   // specified offset is over file size
    }else if (*f_pos + count > file_size) {
        count = file_size - *f_pos; // number of bytes requested are over file size: trim count
    }

    // determine the block level offset for the operation
    offset = *f_pos % AOS_BLOCK_SIZE;
    // just read stuff in a single block - residuals will be managed at the application level
    if (offset + count > AOS_BLOCK_SIZE) count = AOS_BLOCK_SIZE - offset;

    // compute the actual index of the block to be read from device
    block_to_read = (*f_pos / AOS_BLOCK_SIZE) + 1 + ; //the value 2 accounts for superblock and file-inode on device

    printk("%s: read operation must access block %d of the device", MODNAME, block_to_read);

    bh = (struct buffer_head *)sb_bread(filp->f_path.dentry->d_inode->i_sb, block_to_read);
    if(!bh) return -EIO;

    ret = copy_to_user(buf,bh->b_data + offset, count);
    *f_pos += (count - ret);
    brelse(bh);

    return count - ret;
}

/*
 * Searches a directory for an inode corresponding to the filename included in a dentry object.
 * */
static struct dentry *aos_lookup(struct inode *parent_inode, struct dentry *dentry, unsigned int flags){
    struct aos_inode *aos_inode;
    struct super_block *sb = parent_inode->i_sb;
    struct buffer_head *bh = NULL;
    struct inode *inode = NULL;

    printk("%s: running the lookup inode-function for name %s", MODNAME, dentry->d_name.name);

    if (strcmp(dentry->d_name.name, DEVICE_NAME)) return NULL;

    // get a locked inode from the cache
    inode = iget_locked(sb, 1);
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

