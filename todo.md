* TODO: write back policy at compile time
* TODO: chronological reading when calling read() on the device (aos_read)
  * IDEA: using block's metadata to identify a chronological delivery order.


* CHECK: the valid bit is set correctly in INVALIDATE syscall
* CHECK: invalidate and get use the reader seqlock, taking the buffer head in the loop. Need to release BH each trial.


* TEST: use only checks on free map when possible.
* TEST: avoid using the valid metadata.