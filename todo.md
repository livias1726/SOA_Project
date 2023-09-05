* TODO: chronological reading when calling read() on the device (aos_read)
  * IDEA: using block's metadata to identify a chronological delivery order.

* TEST: use only checks on free map when possible. 
  * -> GET: reading block to retrieve availability sets an order between
            operations and can trigger a retry if an invalidation is done before.
  * TEST: avoid using the valid metadata.
    * -> maybe impossible.