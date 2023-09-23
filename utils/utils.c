#include <linux/seqlock.h>
#include <linux/buffer_head.h>

#include "../include/config.h"
#include "../include/aos_fs.h"
#include "../include/utils.h"

extern aos_fs_info_t *info;

/*
 * Inserts a new message in the given block, updating its metadata
 * Used by:
 * - PUT operations to insert a new block in the chain.
 * */
int put_new_block(int blk, char* source, size_t size, int prev){
    struct buffer_head *bh;
    struct aos_data_block *data_block;
    int pc;
    size_t ret;

    bh = sb_bread(info->vfs_sb, blk);
    if(!bh) return -EIO;
    data_block = (struct aos_data_block*)bh->b_data;

    /* When more than one thread is allowed to write on the same block
     * it always happens on different data/metadata, so that writers can operate concurrently.
     * There's no need for each writer to call write_seqlock as this will block each operation uselessly.
     * To get the write_lock on the block and release it, it is used a counter variable:
     * - If the variable was 0 before increasing its value, take the write_lock.
     * - If the variable becomes 0 after decreasing its value, release the write_lock. */
    pc = __sync_fetch_and_add(&data_block->metadata.counter, 1);
    if (!pc) write_seqlock(&info->block_locks[blk]); // the first to operate on the block takes the write lock

    /* Write block on device */
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;
    data_block->metadata.prev = prev;

    pc = __sync_sub_and_fetch(&data_block->metadata.counter, 1);
    if (!pc) { // the last to operate on the block releases the write lock
        mark_buffer_dirty(bh);
        WB { sync_dirty_buffer(bh); } // immediate synchronous write on the device
        write_sequnlock(&info->block_locks[blk]);
    }

    brelse(bh);

    return size;
}

/*
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
int invalidate_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh){
    int pc, fail;
    uint64_t prev, next;
    bool is_first, is_first_or_last;

    pc = __sync_fetch_and_add(&data_block->metadata.counter, 1);
    if (!pc) write_seqlock(&info->block_locks[blk]); // the first to operate on the block takes the write lock

    /* Change the block's metadata */
    data_block->metadata.is_valid = 0;
    prev = data_block->metadata.prev;
    next = data_block->metadata.next;

    /* If blk is 'first' then change 'first' to 'next'*/
    is_first = __sync_bool_compare_and_swap(&info->first, blk, next);
    if (is_first) {
        /* If blk is 'first' AND 'last' then change 'last' to 1 */
        is_first_or_last = (__sync_bool_compare_and_swap(&info->last, blk, 1) || is_first);
    } else {
        /* If blk is 'last' but not 'first' then change 'last' to 'prev' */
        is_first_or_last = (__sync_bool_compare_and_swap(&info->last, blk, prev) || is_first);
    }

    /* Else, update previous and next block */
    if (!is_first_or_last) {
        /* Write on previous block */
        fail = change_next_block(prev, next);
        if (fail < 0) return fail;

        /* Write on successive block */
        fail = change_prev_block(next, prev);
        if (fail < 0) return fail;
    }

    pc = __sync_sub_and_fetch(&data_block->metadata.counter, 1);
    if (!pc) { // the last to operate on the block releases the write lock
        mark_buffer_dirty(bh);
        write_sequnlock(&info->block_locks[blk]);
    }

    brelse(bh);

    return 0;
}

/*
 * Updates the 'next' variable in the metadata of the given block.
 * Used by:
 * - PUT operations to chronologically link a block to the next one.
 * - INV operations to logically remove a block from the chain of readable blocks.
 * */
int change_next_block(int blk, int next_blk){
    struct buffer_head *bh_prev;
    struct aos_data_block *prev_block;
    int pc;

    bh_prev = sb_bread(info->vfs_sb, blk);
    if(!bh_prev) return -EIO;

    prev_block = (struct aos_data_block*)bh_prev->b_data;

    /* Change previous block's 'next' metadata */
    pc = __sync_fetch_and_add(&prev_block->metadata.counter, 1);
    if (!pc) write_seqlock(&info->block_locks[blk]);

    prev_block->metadata.next = next_blk;

    pc = __sync_sub_and_fetch(&prev_block->metadata.counter, 1);
    if (!pc) {
        mark_buffer_dirty(bh_prev);
        write_sequnlock(&info->block_locks[blk]);
    }

    brelse(bh_prev);

    return 0;
}

/*
 * Updates the 'prev' variable in the metadata of the given block.
 * Used by:
 * - INV operations to logically remove a block from the chain of readable blocks.
 * */
int change_prev_block(int blk, int prev_blk){
    struct buffer_head *bh_next;
    struct aos_data_block *next_block;
    int pc;

    bh_next = sb_bread(info->vfs_sb, blk);
    if (!bh_next) return -EIO;

    next_block = (struct aos_data_block*)bh_next->b_data;

    /* Change previous block's 'next' metadata */
    pc = __sync_fetch_and_add(&next_block->metadata.counter, 1);
    if (!pc) write_seqlock(&info->block_locks[blk]);

    next_block->metadata.prev = prev_blk;

    pc = __sync_sub_and_fetch(&next_block->metadata.counter, 1);
    if (!pc) {
        mark_buffer_dirty(bh_next);
        write_sequnlock(&info->block_locks[blk]);
    }

    brelse(bh_next);

    return 0;
}