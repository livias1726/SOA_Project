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
* TEST: what happens if a thread dies before closing the device (without subtracting the presence counter)

* PROBLEMA VERSIONE 2 INVALIDATE: è troppo veloce rispetto al PUT -> va ad invalidare un blocco quando trova il bit
    settato dalla put anche se questa non ha ancora completato la scrittura: viene eseguita una scrittura su un blocco che
    è già invalidato:
  * SOLUZIONI: 
    * effettuare set_bit in PUT solo alla fine -> possibili conflitti tra più PUT (da testare)

* TEST: se is_valid non viene utilizzato nella GET, diventa inutile!!!
    