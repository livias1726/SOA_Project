#include <linux/init.h>
#include <linux/module.h>
#include <linux/timekeeping.h>
#include <linux/time.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>
#include <linux/version.h>

#include "../include/aos_fs.h"
#include "../include/config.h"

/**
 * This module implements file system specific operations, such as the mount and unmount utilities and the function
 * to fill the superblock and the 'info' structure.
 * */

static struct super_operations aos_sb_ops = {
};

static struct dentry_operations aos_de_ops = {
};

aos_fs_info_t *info;
uint64_t is_mounted = 0;

static int init_fs_info(struct aos_super_block* aos_sb) {

    int nblocks = aos_sb->partition_size;
    int longs = BITS_TO_LONGS(nblocks);     /* Number of unsigned longs needed to cover nblocks bits */
    int i;

    /* Allocate bitmaps */
    info->free_blocks = kzalloc(longs * sizeof(long), GFP_KERNEL);
    if (!info->free_blocks) {
        printk(KERN_ALERT "%s: [init_fs_info()] couldn't allocate free blocks bitmap\n", MODNAME);
        goto fail_1;
    }
    info->put_map = kzalloc(longs * sizeof(long), GFP_KERNEL);
    if (!info->put_map) {
        printk(KERN_ALERT "%s: [init_fs_info()] couldn't allocate PUT bitmap\n", MODNAME);
        goto fail_2;
    }
    info->inv_map = kzalloc(longs * sizeof(long), GFP_KERNEL);
    if (!info->inv_map) {
        printk(KERN_ALERT "%s: [init_fs_info()] couldn't allocate INVALIDATE bitmap\n", MODNAME);
        goto fail_3;
    }

    /* Restore state information from the Superblock */
    bitmap_or(info->free_blocks, info->free_blocks, aos_sb->padding, nblocks);
    info->first = aos_sb->first;
    info->last = aos_sb->last;

    /* Init every seqlock associated to each block */
    info->block_locks = kzalloc(nblocks * sizeof(seqlock_t), GFP_KERNEL);
    if (!info->block_locks) {
        printk(KERN_ALERT "%s: [init_fs_info()] couldn't allocate seqlocks\n", MODNAME);
        goto fail_4;
    }

    for (i = 0; i < nblocks; ++i) { seqlock_init(&info->block_locks[i]); }

    info->vfs_sb->s_fs_info = info;

    return 0;

    fail_4:
        kfree(info->inv_map);
    fail_3:
        kfree(info->put_map);
    fail_2:
        kfree(info->free_blocks);
    fail_1:
        return -ENOMEM;
}

/**
 * This function is called to terminate the superblock initialization, which involves filling the
 * struct super_block structure fields and the initialization of the root directory inode.
 * The maximum number of manageable blocks is a parameter NBLOCKS that can be configured at compile time.
 * If a block-device layout keeps more than NBLOCKS blocks, the mount operation of the device should fail.
 * */
static int aos_fill_super(struct super_block *sb, void *data, int silent) {

    struct inode *root_inode;
    struct buffer_head *bh;
    struct timespec64 curr_time;
    int fail;

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

        fail = -EIO;
        goto failure_1;
    }
    memcpy(&info->sb, bh->b_data, AOS_BLOCK_SIZE);
    brelse(bh);

    /* Check magic number */
    if(info->sb.magic != MAGIC) {
        printk(KERN_ALERT "%s: [aos_fill_super()] incorrect magic number. abort mounting.\n", MODNAME);

        fail = -EBADF;
        goto failure_1;
    }

    /* Fill superblock */
    sb->s_magic = MAGIC;
    sb->s_type = &aos_fs_type;
    sb->s_op = &aos_sb_ops;

    info->vfs_sb = sb;
    if(init_fs_info(&info->sb)) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't initialize aos_fs_info structure\n", MODNAME);

        fail = -ENOMEM;
        goto failure_1;
    }

    /* Get a VFS inode for the root directory */
    root_inode = iget_locked(sb, SUPER_BLOCK_IDX);
    if (!root_inode) {
        printk(KERN_ALERT "%s: [aos_fill_super()] couldn't lock the root inode\n", MODNAME);

        fail = -ENOMEM;
        goto failure_2;
    }

    if (root_inode->i_state & I_NEW) { // created a new inode
        root_inode->i_ino = ROOT_INODE_NUMBER;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,12,0)
        inode_init_owner(&init_user_ns, root_inode, NULL, S_IFDIR);
#else
        inode_init_owner(root_inode, NULL, S_IFDIR);
#endif

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

        fail = -ENOMEM;
        goto failure_2;
    }
    sb->s_root->d_op = &aos_de_ops;

    is_mounted = 1;

    // unlock the inode to make it usable
    unlock_new_inode(root_inode);

    return 0;

failure_2:
    kfree(info->free_blocks);
    kfree(info->put_map);
    kfree(info->inv_map);
    kfree(info->block_locks);
failure_1:
    kfree(info);
    return fail;
}

static void aos_kill_superblock(struct super_block *sb){
    struct buffer_head *bh;
    struct aos_super_block *aos_sb;
    int trials = 0;

    /* Atomically set the device as unmounted, to stop every new thread trying to access the device */
    __atomic_store_n(&is_mounted, 0, __ATOMIC_RELAXED);

    /* Wait for every thread already in the device to complete */
    while (info->counter && trials < SYSCALL_TRIALS) {
        wait_event_interruptible_timeout(wq, (info->counter == 0), msecs_to_jiffies(JIFFIES));
        printk(KERN_INFO "%s: waiting to unmount...\n", MODNAME);
        trials++;
    }

    /* Read superblock */
    bh = sb_bread(sb, SUPER_BLOCK_IDX);
    if(!bh) {
        printk(KERN_ALERT "%s: [aos_kill_super()] couldn't save device info in the vfs superblock\n", MODNAME);
        goto failure;
    }
    aos_sb = (struct aos_super_block*)bh->b_data;
    /* Save FS info */
    aos_sb->first = info->first;
    aos_sb->last = info->last;
    memcpy(aos_sb->padding, info->free_blocks, BITS_TO_BYTES(aos_sb->partition_size));

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

failure:
    kfree(info->free_blocks);
    kfree(info->put_map);
    kfree(info->inv_map);
    kfree(info->block_locks);
    kfree(info);

    kill_block_super(sb);

    printk(KERN_INFO "%s: Unmount complete.\n", MODNAME);
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
