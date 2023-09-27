* Invalidate checking on if 'offset' is 'last':
  * Non-atomic condition
  * Assumes that the PUT that firstly set the bit is also the PUT that firstly changes 'last'
  

* PUT needs to lock last to avoid conflicts with INV after changing 'last':
  * INV will see this as ENODATA, even though it will be better if it's just a wait on bit.
  

* ~~Read checks for invalidations during a reading, but if INV zeroes out the prev and next metadata,~~
  * ~~the reading chain will break, causing errors such as reading the superblock as a normal block.~~


* When re-installing the modules without clearing the device, information on the blocks remains intact 
  * but device info (first, last, ecc) does not.
    * To save those info, they need to be maintained in the superblock or saved otherwise at the unmounting.


# TEST #

1. Limit to longs*32 when building the bitmap
2. Constant bitmaps (not dynamically allocated)