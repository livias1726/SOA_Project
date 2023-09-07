* TODO: chronological reading when calling read() on the device (aos_read)
  * IDEA: using block's metadata to identify a chronological delivery order.

* TEST: use only checks on free map when possible. 
  * -> GET: reading block to retrieve availability sets an order between
            operations and can trigger a retry if an invalidation is done before.
  * TEST: avoid using the valid metadata.
    * -> maybe impossible.
* TEST: GET: if validity is checked from bitmap before reading, it will be preserved the starting order of writings
            meaning that if an invalidation is performed while reading, this will not invalidate the get operation.
            This does not happen when the validity is checked from metadata after the reading.