* TEST: use only checks on free map when possible (avoid using the valid metadata)
  * Reading block to retrieve availability sets an order between operations and can trigger a retry if an 
    invalidation is done before.
    * If validity is checked from bitmap before reading, it will be preserved the starting order of writings
    meaning that if an invalidation is performed while reading, this will not invalidate the get operation.
    This does not happen when the validity is checked from metadata after the reading.

# TEST ALGORITMI DI LETTURA

Using metadata: ogni blocco ha nei metadati l'indice del proprio successore e precedente.

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
  * On same block: PUT sets bit in put_mask -> INV is rejected (trying to invalidate a message not yet created)
  * **On different blocks: ???**
* INV-PUT: 
  * se INV sta operando su last, PUT deve attendere che abbia finito così da prelevare il nuovo valore di last 
* INV-INV: 
  * ordinate con test_and_clear per lo stesso blocco. INV concorrenti non possono operare sul blocco precedente e 
          successivo a quello in invalidazione. Devono attendere la fine della precedente.

### Write Seqlocks handling
* When more than 1 thread writes on the same block but on different metadata, writers can operate concurrently.
* To get the write_lock on the block and release it, it can be used a counter variable: 
  * If the variable was 0 before increasing its value, take the write_lock.
  * If the variable becomes 0 after decreasing its value, release the write_lock.
* Writers that can be concurrent won't block each other.
