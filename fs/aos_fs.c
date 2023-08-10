#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "include/aos_fs.h"

/**
 * The device should be mounted on whichever directory of the file system to enable the operations by threads.
 * It is assumed that the device driver can support a single mount at a time.
 * */

static struct super_operations aos_sbops = {
};

static struct dentry_operations aos_eops = {
};

/*
 * This function is called to terminate the superblock initialization, which involves filling the
 * struct super_block structure fields and the initialization of the root directory inode.
 *
 * The maximum number of manageable blocks is a parameter NBLOCKS that can be configured at compile time.
 * If a block-device layout keeps more than NBLOCKS blocks, the mount operation of the device should fail.
 * */
static int aos_fill_super(struct super_block *sb, void *data, int silent) {

    aos_fs_info_t *info;
    struct aos_super_block *aos_sb;
    struct inode *root_inode;
    struct buffer_head *bh;
    struct timespec64 curr_time;
    uint64_t magic;

    /* Updating the VFS super_block */
    sb->s_magic = MAGIC;
    bh = sb_bread(sb, SUPER_BLOCK_IDX);
    if(!bh) return -EIO;
    aos_sb = (struct aos_super_block *)bh->b_data;
    magic = aos_sb->magic;
    brelse(bh);

    if(magic != sb->s_magic) return -EBADF;

    sb->s_type = &aos_fs_type; // file_system_type
    sb->s_op = &aos_sbops; // super block operations

    /* Prepare AOS FS specific super block */
    if (!(info = (aos_fs_info_t *)(kzalloc(sizeof(aos_fs_info_t), GFP_KERNEL)))) return -ENOMEM;
    info->vfs_sb = sb;
    info->sb = aos_sb;
    sb->s_fs_info = info;

    // get a VFS inode for the root directory
    root_inode = iget_locked(sb, 0);
    if (!root_inode) {
        kfree(info);
        return -ENOMEM;
    }

    if (root_inode->i_state & I_NEW) { // created a new inode
        root_inode->i_ino = ROOT_INODE_NUMBER;
        inode_init_owner(root_inode, NULL, S_IFDIR);
        root_inode->i_sb = sb;
        root_inode->i_op = &aos_inode_ops;
        root_inode->i_fop = &aos_dir_ops;
        root_inode->i_mode = (S_IFDIR | S_IRWXU | S_IROTH | S_IXOTH);
        ktime_get_real_ts64(&curr_time);
        root_inode->i_atime = root_inode->i_mtime = root_inode->i_ctime = curr_time;

        // no inode from device is needed - the root of our file system is an in memory object
        root_inode->i_private = NULL;
    }

    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        iget_failed(root_inode);
        kfree(info);
        return -ENOMEM;
    }

    // unlock the inode to make it usable
    unlock_new_inode(root_inode);

    /* Make the VFS superblock point to the dentry. */
    sb->s_root->d_op = &aos_eops;

    info->is_mounted = 1;

    return 0;
}

static void aos_kill_superblock(struct super_block *sb){
    // todo: wait for pending operations
    // todo: deallocate structures

    kill_block_super(sb);
}

struct dentry *aos_mount(struct file_system_type *fs_type, int flags, const char *dev_name, void *data) {

    struct dentry *ret;

    ret = mount_bdev(fs_type, flags, dev_name, data, aos_fill_super);

    if (unlikely(IS_ERR(ret))) {
        printk(KERN_ERR "%s: mount failure", MODNAME);
    }else {
        printk(KERN_INFO "%s: file system mounted on [%s]\n", MODNAME, dev_name);
    }
    return ret;
}

struct file_system_type aos_fs_type = {
    .owner = THIS_MODULE,
    .name = "aos_fs",
    .mount = aos_mount,
    .kill_sb = aos_kill_superblock
};
