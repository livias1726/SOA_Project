* Invalidate checking on if 'offset' is 'last':
  * Non-atomic condition
  * Assumes that the PUT that firstly set the bit is also the PUT that firstly changes 'last'
  

* PUT needs to lock last to avoid conflicts with INV after changing 'last':
  * INV will see this as ENODATA, even though it will be better if it's just a wait on bit.