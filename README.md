# TODO #

* Test invalidate without changing metadata: when PUT takes a block and its metadata are not 0,
    it takes those blocks and changes metadata to point to each other.
  * NO INVALIDATION CONFLICTS -> last is updated atomically as is the bitmap
    * It could write on is_valid
    * If is_valid is not used, the bitmap should be used to read data.
  * PUT CONFLICTS -> if last is updated A->B but blocks metadata is updated B->A, it could break


* Quando put legge che 'prev' è 1 -> seleziona 'next' come nuovo 'first'. Invalidate non si occupa di 
    aggiornare 'first'! (analizza i casi in cui inv aggiorna first e/o last)