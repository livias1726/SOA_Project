# Conflicts

# Notes

* Invalidate checking on if 'offset' is 'last':
  * Non-atomic condition
  * Assumes that the PUT that firstly set the bit is also the PUT that firstly changes 'last'
  * If a PUT changes 'last' but an INV on 'last' executes after this but before the PUT ends:
    * INV cannot be stopped
      * It's not operating on the same block as PUT
      * Doesn't know it's executing on the real 'last', so doesn't wait the end of the PUT
    * PUT is executing before the INV, so it doesn't detect an ongoing INV
  * /////////////////////////////////////////////////////////////////
  * When executing, the PUT needs to lock last 
* Counter metadata
  * It's read a single time, it's not written in memory and most probably it's not correct for its use -> test of 
    performance making everybody get the write_lock

