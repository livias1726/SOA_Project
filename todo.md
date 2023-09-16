* TEST: use only checks on free map when possible (avoid using the valid metadata)
  * Reading block to retrieve availability sets an order between operations and can trigger a retry if an 
    invalidation is done before.
    * If validity is checked from bitmap before reading, it will be preserved the starting order of writings
    meaning that if an invalidation is performed while reading, this will not invalidate the get operation.
    This does not happen when the validity is checked from metadata after the reading.
  
* TEST: what happens if a thread dies before closing the device (without subtracting the presence counter)

* PROBLEMA: in un contesto multi-thread, è possibile che la prima scrittura non venga eseguita sul blocco 2, 
    rendendo inutile la gestione di 'first' in invalidate.
* PROBLEMA: in PUT concorrenti viene saltato il blocco 5
* PROBLEMA: se INV invalida un indice (es: 3), ma non l'indice precedente (es: 2) e, successivamente, PUT scrive
    in ogni blocco fino a tornare a scrivere nel primo indice invalidato (3), la READ andrà a leggere 3 come successivo 
    a 2, quando, in realtà, andrebbe letto per ultimo.

* TEST: bitmap ops before or after block ops
    