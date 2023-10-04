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
    struct buffer_head *bh_next = NULL;
    struct aos_data_block *next_block;
    int fail;

    write_seqlock(&info->block_locks[blk]);

    fail = get_blk(bh_next, info->vfs_sb, blk, &next_block);
    if (fail < 0) goto failure;

    next_block->metadata.prev = prev_blk;

    mark_buffer_dirty(bh_next);
    brelse(bh_next);

failure:
    write_sequnlock(&info->block_locks[blk]);
    return fail;
}

/*
 * Updates the 'next' variable in the metadata of the given block.
 * Used by:
 * - PUT operations to chronologically link a block to the next one.
 * - INV operations to logically remove a block from the chain of readable blocks.
 * */
static int change_block_next(int blk, int next_blk){
    struct buffer_head *bh_prev = NULL;
    struct aos_data_block *prev_block;
    int fail;

    write_seqlock(&info->block_locks[blk]);

    fail = get_blk(bh_prev, info->vfs_sb, blk, &prev_block);
    if (fail < 0) goto failure;

    prev_block->metadata.next = next_blk;

    mark_buffer_dirty(bh_prev);
    brelse(bh_prev);

failure:
    write_sequnlock(&info->block_locks[blk]);
    return fail;
}

/*
 * Updates both the 'prev' and 'next' variable in the metadata of the given block.
 * Used by INV operations to logically remove a block from the chain of readable blocks.
 * */
int change_blocks_metadata(int prev_blk, int next_blk){
    struct buffer_head *bh_next = NULL, *bh_prev = NULL;
    struct aos_data_block *next_block, *prev_block;
    int fail;

    if (prev_blk == 1) {
        fail = change_block_prev(next_blk, 1);
        return fail;
    } else if (next_blk == 0) {
        fail = change_block_next(prev_blk, 0);
        return fail;
    }

    /*
    write_seqlock(&info->block_locks[prev_blk]);

    fail = get_blk(bh_prev, info->vfs_sb, prev_blk, &prev_block);
    if (fail < 0) goto failure_1;

    write_seqlock(&info->block_locks[next_blk]);

    fail = get_blk(bh_next, info->vfs_sb, next_blk, &next_block);
    if (fail < 0) goto failure_2;

    next_block->metadata.prev = prev_blk;
    prev_block->metadata.next = next_blk;

    mark_buffer_dirty(bh_prev);
    mark_buffer_dirty(bh_next);

    brelse(bh_prev);
    brelse(bh_next);

    failure_2:
        write_sequnlock(&info->block_locks[next_blk]);
    failure_1:
        write_sequnlock(&info->block_locks[prev_blk]);
        return fail;
    */

    /* PREV ---------------------------------------- */
    write_seqlock(&info->block_locks[prev_blk]);

    fail = get_blk(bh_prev, info->vfs_sb, prev_blk, &prev_block);
    if (fail < 0) {
        write_sequnlock(&info->block_locks[prev_blk]);
        return fail;
    }

    prev_block->metadata.next = next_blk;

    mark_buffer_dirty(bh_prev);
    brelse(bh_prev);

    write_sequnlock(&info->block_locks[prev_blk]);
    /* ---------------------------------------- PREV */

    /* NEXT ---------------------------------------- */
    write_seqlock(&info->block_locks[next_blk]);

    fail = get_blk(bh_next, info->vfs_sb, next_blk, &next_block);
    if (fail < 0) {
        write_sequnlock(&info->block_locks[next_blk]);
        return fail;
    }
    next_block->metadata.prev = prev_blk;

    mark_buffer_dirty(bh_next);
    brelse(bh_next);

    write_sequnlock(&info->block_locks[next_blk]);
    /* ---------------------------------------- NEXT */

    return fail;
}

/**
 * Inserts a new message in the given block, updating its metadata
 * */
int put_new_block(int blk, char* source, size_t size, int prev, int *old_first){
    struct buffer_head *bh = NULL;
    struct aos_data_block *data_block;
    int res;
    size_t ret;

    /* If the block is the first to be written (prev is 1), update 'first' */
    if (prev == 1) {
        *old_first = __atomic_exchange_n(&info->first, blk, __ATOMIC_RELAXED);
    } else{
        /* Writes on the 'next' metadata of the previous block (old_last) */
        res = change_block_next(prev, blk);
        if (res < 0) goto failure_1;
    }

    write_seqlock(&info->block_locks[blk]);

    res = get_blk(bh, info->vfs_sb, blk, &data_block);
    if(res < 0) goto failure_2;

    // Write block on device
    ret = copy_from_user(data_block->data.msg, source, size);
    size -= ret;
    data_block->data.msg[size+1] = '\0';
    data_block->metadata.is_valid = 1;
    data_block->metadata.prev = prev;

    mark_buffer_dirty(bh);
    WB { sync_dirty_buffer(bh); } // immediate synchronous write on the device
    brelse(bh);

    res = size;

    failure_2:
        write_sequnlock(&info->block_locks[blk]);
    failure_1:
        return res;
}

void invalidate_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh){

    write_seqlock(&info->block_locks[blk]);
    data_block->metadata.is_valid = 0;
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    write_sequnlock(&info->block_locks[blk]);

    brelse(bh);
}

