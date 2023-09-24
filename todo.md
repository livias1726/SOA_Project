# CHRONOLOGICAL READ

Using metadata: each block maintains a reference to its predecessor 
(to update 'last' when the last block of the chronological chain is invalidated) and successor (to read chronologically)

## PUT 
1. Check device and params.
2. Test_and_Set the first free block. If no free blocks, return. -> No PUT on the same block.
3. Set PUT usage on blk_idx. If last is used by INV, then wait -> No INV conflicts 
4. CAS on last and retrieve old_last. -> No conflicts on last for concurrent PUT.
5. Write new block (msg, is_valid, prev).
6. If old_val = 1, then change first = blk_idx.
7. Else, write preceding block (next).
8. Clear PUT usage on blk_idx.

## INV
1. Check device and params.
2. Test_and_Set INV usage on blk_idx -> if found set, then another INV on the same block is already pending. 
-> No INV on the same block.
3. If blk_idx is used by PUT, return. If blk_idx.prev is used by PUT, wait -> No PUT conflict.
4. Test_and_clear blk_idx on free block. If already cleared, return -> No INV on the same block. (REDUNDANT)
5. If blk_idx.prev and blk_idx.next are used by INV, wait -> No conflicts on concurrent INV.
6. Invalidate new block (is_valid).
7. If blk_idx = first AND blk_idx = last, CAS on last = 1 and return. 
8. If blk_idx = first, CAS on first = blk_idx.next and return.
9. If blk_idx = last, CAS on last = blk_idx.prev and return.
10. Write preceding block (next = blk_idx.next) and successive block (prev = blk_idx.prev)
11. Clear blk_idx bit in INV_MASK.

### Conflicts
* PUT-PUT -> put always appends!
  * On same block: test_and_set loop.
  * On different blocks: CAS on last -> writings on the same block will touch different metadata (order doesn't matter) 
* PUT-INV -> INV on the same block or on 'last'
  * On same block: PUT sets the bit in put_mask -> INV is rejected (trying to invalidate a message not yet created)
  * **On different blocks: ???**
* INV-PUT: 
  * On 'last', any PUT needs to wait to use a correct value for 'last'.  
  * On other blocks, no conflicts -> if the new next is 'last', it will be changed its 'prev' while PUT writes on 'next'.
* INV-INV: 
  * On same block: test_and_set loop on INV_MAP.
  * On different blocks: generates conflicts when operating concurrently on a 'prev' or 'next' used by another INV.

