#ifndef SOA_PROJECT_UTILS_H
#define SOA_PROJECT_UTILS_H

int put_new_block(int blk, char* source, size_t size, int prev, int *old_first);
int invalidate_first_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh);
int invalidate_last_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh);
int invalidate_middle_block(int blk, struct aos_data_block *data_block, struct buffer_head *bh);

static inline int wait_inv(ulong *map, int bit){
    bool wait;
    int trials = 0;
    do{
        wait = wait_on_bit_timeout(map, bit, TASK_INTERRUPTIBLE, JIFFIES);
        if (trials > SYSCALL_TRIALS) return -EAGAIN;
        trials++;
    } while (wait);

    return 0;
}

#endif //SOA_PROJECT_UTILS_H
