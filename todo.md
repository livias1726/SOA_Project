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


# TEST ALGORITMI DI LETTURA
1) Lista collegata di structs {int idx, *next} su cui PUT effettua l'append ad ogni nuova scrittura e INV effettua la 
    remove ad ogni invalidazione
   - PUT inserisce sempre alla fine 
     - Conflitti con altre PUT: necessario sequenzializzare gli accessi alla lista 
     - Conflitti con INV se questo opera sull'ultimo nodo della lista allo stesso tempo della PUT 
   - INV deve scansionare la lista per rimuovere il nodo con IDX = OFFSET
     - Collo di bottiglia con NBLOCKS molto grande.
     - Conflitti con altre INV se operano su blocchi adiacenti in lista.
     - Conflitti con PUT se opera sull'ultimo blocco.
   - Possibile utilizzare i seqlock associati al blocco modificato per operare sul nodo in lista? Non risolve i conflitti 
     su nodi adiacenti, a meno che non vengano presi i seqlock anche per quelli. 
   - La lettura diventa banale se non si tocca la lista durante le operazioni. Altrimenti, serve RCU per i lettori.


2) Using metadata: ogni blocco ha nei metadati l'indice del proprio successore e precedente
   - Ogni PUT modifica 2 blocchi, 2 variabili e la bitmap: 
     - il blocco nuovo (msg, is_valid, prev) 
     - il suo predecessore, che corrisponde a 'last' (next)
     - la variabile first (SE E SOLO SE 'last' = 1) da settare al nuovo blocco
     - la variabile last da settare al nuovo blocco
     - il bit relativo al nuovo blocco (set)
       - Ottimizzazioni:
          - Se il nuovo blocco è il primo inserito ('last' = 1) allora non serve modificare nessun altro blocco 
          - Se il blocco da invalidare è 'last' allora non serve modificare nessun altro blocco (la lettura verifica
            sempre anche bitmap o validity bit)
   - Ogni INV modifica 3 blocchi, 2 variabili e la bitmap: 
     - il blocco da invalidare (is_valid = 0) -> ricava predecessore e successore
     - il suo predecessore (next = successore) 
     - il suo successore (prev = predecessore)
     - la variabile last (SE E SOLO SE è uguale al blocco da invalidare), per settarla al predecesore
     - la variabile first (SE E SOLO SE è uguale al blocco da invalidare), per settarla al successore
     - il bit relativo al blocco invalidato (clear)
       - Ottimizzazioni:
         - Se il blocco da invalidare è 'first' allora non serve modificare nessun altro blocco (??)
         - Se il blocco da invalidare è 'last' allora non serve modificare nessun altro blocco (la lettura verifica 
         sempre anche bitmap o validity bit)
         - Se il blocco da invalidare è sia first che last, allora last si imposta a 1.
