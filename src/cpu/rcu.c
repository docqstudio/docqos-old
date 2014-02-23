#include <core/const.h>
#include <cpu/atomic.h>
#include <cpu/rcu.h>

int lockRCUReadLock(RCULock *lock)
{
   atomicAdd(&lock->count,1);
              /*Just add the count.*/
   return 0;
}

int unlockRCUReadLock(RCULock *lock)
{
   if(atomicAddRet(&lock->count,-1) == 0)
   {   /*This lock is unlocked.*/
      lockSpinLock(&lock->lock);
      for(int i = 0;i < lock->ccount;++i)
         (*lock->callbacks[i])(lock->data[i]);
      lock->ccount = 0;     /*Call the callbacks.*/
      unlockSpinLock(&lock->lock);
   }
   return 0;
}

int addRCUCallback(RCULock *lock,RCUCallback *callback,void *data)
{
   int retval = -EBUSY;
   if(atomicRead(&lock->count) == 0)
      return (*callback)(data); /*Unlocked,just call it.*/

   lockSpinLock(&lock->lock);
   if(atomicRead(&lock->count) == 0)
      goto call; /*Make sure this lock is locked again.*/

   if(lock->ccount == sizeof(lock->callbacks) / sizeof(lock->callbacks[0]))
      goto out;
   lock->data[lock->ccount] = data;
   lock->callbacks[lock->ccount++] = callback; /*Add the callback.*/
   retval = 0;
out:
   unlockSpinLock(&lock->lock);
   return retval;
call:
   unlockSpinLock(&lock->lock);
   return (*callback)(data);
}
