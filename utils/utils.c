#include <linux/seqlock.h>
#include <linux/buffer_head.h>

#include "../include/config.h"
#include "../include/aos_fs.h"
#include "../include/utils.h"

extern aos_fs_info_t *info;

/*
 * Updates the 'prev' variable in the metadata of the given block.
 * Used by INV operations to logically remove a 'first' block from the chain of readable blocks.
 * */
static int change_block_prev(int blk, int prev_blk){
    struct buffer_head *bh_next;
    struct aos_data_block *next_block;

    bh_next = sb_bread(info->vfs_sb, blk);
    if (!bh_next) return -EIO;
    next_block = (struct aos_data_block*)bh_next->b_data;

    //write_seqlock(&info->block_locks[blk]);
    next_block->metadata.prev = prev_blk;
    mark_buffer_dirty(bh_next);
    //write_sequnlock(&info->block_locks[blk]);
    brelse(bh_next);

    return 0;
}

/*
 * Updates the 'next' variable in the metadata of the given block.
 * Used by:
 * - PUT operations to chronologically link a block to the next one.
 * - INV operations to logically remove a block from the chain of readable blocks.
 * */
static int change_block_next(int blk, int next_blk){
    struct buffer_head *bh_prev;
    struct aos_data_block *prev_block;

    bh_prev = sb_bread(info->vfs_sb, blk);
    if(!bh_prev) return -EIO;
    prev_block = (struct aos_data_block*)bh_prev->b_data;

    //write_seqlock(&info->block_locks[blk]);
    prev_block->metadata.next = next_blk;
    mark_buffer_dirty(bh_prev);
    //write_sequnlock(&info->block_locks[blk]);
    brelse(bh_prev);

    return 0;
}

/*
 * Updates both the 'prev' and 'next' variable in the metadata of the given block.
 * Used by INV operations to logically remove a block from the chain of readable blocks.
 * */
int change_blocks_metadata(int prev_blk, int next_blk){
    struct buffer_head *bh_next, *bh_prev;
    struct aos_data_block *next_block, *prev_block;
    int fail;

    if (prev_blk == 1) {
        fail = change_block_prev(next_blk, 1);
        return fail;
    } else if (next_blk == 0) {
        fail = change_block_next(prev_blk, 0);
        return fail;
    }

    bh_prev = sb_bread(info->vfs_sb, prev_blk);
    if(!bh_prev) return -EIO;
    prev_block = (struct aos_data_block*)bh_prev->b_data;

    bh_next = sb_bread(info->vfs_sb, next_blk);
    if (!bh_next) return -EIO;
    next_block = (struct aos_data_block*)bh_next->b_data;

    //write_seqlock(&info->block_locks[prev_blk]);
    prev_block->metadata.next = next_blk;
    mark_buffer_dirty(bh_prev);
    //write_sequnlock(&info->block_locks[prev_blk]);
    brelse(bh_prev);

    //write_seqlock(&info->block_locks[next_blk]);
    next_block->metadata.prev = prev_blk;
    mark_buffer_dirty(bh_next);
    //write_sequnlock(&info->block_locks[next_blk]);
    brelse(bh_next);

    return 0;
}

/**
 * Inserts a new message in the given block, updating its metadata
 * */
int put_new_block(int blk, char* source, size_t size, int prev, int *old_first){
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    int res;
    size_t ret;

    bh = sb_bread(info->vfs_sb, blk);
    if(!bh) return -EIO;
    data_block = (struct aos_data_block*)bh->b_data;

    /* If the block is the first to be written (prev is 1), then there's no need to update the previous one
     * to point to the new block. */
    if (prev == 1) {
        *old_first = __atomic_exchange_n(&info->first, blk, __ATOMIC_RELAXED);
        DEBUG { printk(KERN_DEBUG "%s: [put_data() - %d] Atomically swapped 'first' from %d to %d. \n",
                       MODNAME, current->pid, *old_first, blk); }
    } else{
        /* Writes on the 'next' metadata of the previous block (old_last) */
        res = change_block_next(prev, blk);
        if (res < 0) goto failure;
    }

    /* Effective changes in the data block will be done after eventual 'change_next_block()' is successful
     * to avoid needing to abort the operation. */

    //write_seqlock(&info->block_locks[blk]); // the first to operate on the block takes the write lock

    // Write block on device
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;
    data_block->metadata.prev = prev;

    mark_buffer_dirty(bh);
    WB { sync_dirty_buffer(bh); } // immediate synchronous write on the device
    //write_sequnlock(&info->block_locks[blk]);

    res = size;

    failure:
        brelse(bh);
        return res;
}

/**
 * Performs the writings on the device to invalidate the given block.
 * 1. Updates the metadata of the block to invalidate.
 * 2. Checks which case is currently executing (block to invalidate is the first, last, both or neither in the chain).
 *    If the block to invalidate is:
 * 2.1. The first, the 'first' variable needs to be updated with the index of its next block.
 * 2.2. The last, the 'last' variable needs to be updated with the index of its previous block.
 * 2.3. Both the first and the last, the 'last' variable needs to be updated to 1 as a placeholder.
 * 2.4. Neither the first nor the last, the metadata of the previous and next blocks must be
 *      updated to point to one-another and unlink the invalid block.
 * */
void invalidate_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh){

    //write_seqlock(&info->block_locks[blk]);

    /* Change the block's metadata */
    /*
    data_block->metadata.prev = 0;
    data_block->metadata.next = 0;
     */
    data_block->metadata.is_valid = 0;

    mark_buffer_dirty(bh);
    //write_sequnlock(&info->block_locks[blk]);

    brelse(bh);
}

