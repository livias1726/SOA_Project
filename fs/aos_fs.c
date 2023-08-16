#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>

#include "include/aos_fs.h"

/**
 * The device should be mounted on whichever directory of the file system to enable the operations by threads.
 * It is assumed that the device driver can support a single mount at a time.
 * */

static struct super_operations aos_sb_ops = {
};

static struct dentry_operations aos_de_ops = {
};

aos_fs_info_t *info;

static int init_fs_info(struct aos_super_block* aos_sb) {
    // build free blocks bitmap as an array of uint64_t
    int nblocks = aos_sb->partition_size;
    int i, nbytes = (ROUND_UP(nblocks, 64)) * 8;

    info->free_blocks = kzalloc(nbytes, GFP_KERNEL);
    if (!info->free_blocks) {
        printk(KERN_ALERT "%s: [init_fs_info()] couldn't allocate free blocks bitmap\n", MODNAME);
        return -ENOMEM;
    }

    // set first two blocks as used (superblock and inode block)
    SET_BIT(info->free_blocks, 0);
    SET_BIT(info->free_blocks, 1);

    rwlock_init(&info->fb_lock);

    nbytes = nblocks * sizeof(seqlock_t);
    info->block_locks = (seqlock_t *)kzalloc(nbytes, GFP_KERNEL);
    if (!info->block_locks) {
        printk(KERN_ALERT "%s: [init_fs_info()] couldn't allocate seqlocks\n", MODNAME);
        return -ENOMEM;
    }

    for (i = 0; i < nblocks; ++i) {
        seqlock_init(&info->block_locks[i]);
    }

    info->vfs_sb->s_fs_info = info;

    return 0;
}

/*
 * This function is called to terminate the superblock initialization, which involves filling the
 * struct super_block structure fields and the initialization of the root directory inode.
 *
 * The maximum number of manageable blocks is a parameter NBLOCKS that can be configured at compile time.
 * If a block-device layout keeps more than NBLOCKS blocks, the mount operation of the device should fail.
 * */
static int aos_fill_super(struct super_block *sb, void *data, int silent) {

    struct inode *root_inode;
    struct buffer_head *bh;
    struct timespec64 curr_time;

    /* Set block size */
    if(!sb_set_blocksize(sb, AOS_BLOCK_SIZE)) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't set the block size in the vfs superblock\n", MODNAME);
        return -EIO;
    }

    /* Create AOS FS info */
    info = (aos_fs_info_t *)(kzalloc(sizeof(aos_fs_info_t), GFP_KERNEL));
    if (!info) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't allocate aos_fs_info structure\n", MODNAME);
        return -ENOMEM;
    }

    /* Read superblock */
    bh = sb_bread(sb, SUPER_BLOCK_IDX);
    if(!bh) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't read the vfs superblock\n", MODNAME);
        kfree(info);
        return -EIO;
    }
    memcpy(&info->sb, bh->b_data, AOS_BLOCK_SIZE);
    brelse(bh);

    /* Check magic number */
    if(info->sb.magic != MAGIC) {
        printk(KERN_ALERT "%s: [aos_fill_super()] incorrect magic number. abort mounting.\n", MODNAME);
        kfree(info);
        return -EBADF;
    }

    /* Fill superblock */
    sb->s_magic = MAGIC;
    sb->s_type = &aos_fs_type;
    sb->s_op = &aos_sb_ops;

    info->vfs_sb = sb;
    if(init_fs_info(&info->sb)) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't initialize aos_fs_info structure\n", MODNAME);
        kfree(info);
        return -ENOMEM;
    }

    /* Get a VFS inode for the root directory */
    root_inode = iget_locked(sb, SUPER_BLOCK_IDX);
    if (!root_inode) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't lock the root inode\n", MODNAME);
        kfree(info->free_blocks);
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

    /* Make the VFS superblock point to the dentry. */
    sb->s_root = d_make_root(root_inode);
    if (!sb->s_root) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't set up the root dentry\n", MODNAME);
        iget_failed(root_inode);
        kfree(info->free_blocks);
        kfree(info);
        return -ENOMEM;
    }
    sb->s_root->d_op = &aos_de_ops;

    info->is_mounted = 1;
    //fs_info = *info;

    // unlock the inode to make it usable
    unlock_new_inode(root_inode);

    AUDIT { printk(KERN_INFO "%s: superblock fill function returned correctly.\n", MODNAME); }

    return 0;
}

static void aos_kill_superblock(struct super_block *sb){

    // todo: wait for pending operations

    // todo: signal device unmounting

    kfree(info->free_blocks);
    kfree(info);

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
