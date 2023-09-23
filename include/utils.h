#ifndef SOA_PROJECT_UTILS_H
#define SOA_PROJECT_UTILS_H

int change_next_block(int blk, int next_blk);
int put_new_block(int blk, char* source, size_t size, int prev, int *old_first);
int change_prev_block(int blk, int next_blk);
int invalidate_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh);

#endif //SOA_PROJECT_UTILS_H
