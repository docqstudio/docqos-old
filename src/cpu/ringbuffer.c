#include <core/const.h>
#include <cpu/ringbuffer.h>
#include <cpu/io.h>
#include <task/signal.h>

static int waitForRingBuffer(RingBuffer *ring,u64 *rflags)
{
   Task *current = getCurrentTask();
   WaitQueue wait;
      /*When we arrive here,the ring->wait.lock is locked and the interrupt is closed.*/
   initWaitQueue(&wait,current);
   addToWaitQueueLocked(&wait,&ring->wait);
          /*Init the wait queue and add it to ring->wait.*/
   
   current->state = TaskInterruptible;
   unlockSpinLockRestoreInterrupt(&ring->wait.lock,rflags);
   
   schedule(); /*Wait until the ring buffer has data.*/
   
   lockSpinLockCloseInterrupt(&ring->wait.lock,rflags);
   removeFromWaitQueueLocked(&wait);
   
   if(taskSignalPending(current)) /*Interrupted by signals.*/
      return -EINTR;
   return 0;
}

int outRingBuffer(RingBuffer *ring,unsigned int c)
{
   u64 rflags;
   lockSpinLockCloseInterrupt(&ring->wait.lock,&rflags);

   *(unsigned int *)&ring->buffer[ring->writer] = c;
   
   ring->writer += sizeof(unsigned int);
   if(ring->writer == ring->size)
      ring->writer = 0; /*Roll to the start.*/
   if(ring->writer == ring->reader) /*The buffer is full!*/
      ring->reader += sizeof(unsigned int);
             /*We just drop the oldest data.*/
   wakeUpLocked(&ring->wait);
   
   unlockSpinLockRestoreInterrupt(&ring->wait.lock,&rflags);
   return 0;
}

int inRingBuffer(RingBuffer *ring,unsigned int *c)
{
   u64 rflags;
   lockSpinLockCloseInterrupt(&ring->wait.lock,&rflags);
   while(ring->writer == ring->reader) /*No data?*/
      if(waitForRingBuffer(ring,&rflags) == -EINTR)
         goto interrupted;

   *c = *(unsigned int *)&ring->buffer[ring->reader];
            /*Get the data.*/
   ring->reader += sizeof(unsigned int);
   if(ring->reader == ring->size)
      ring->reader = 0; /*Roll to start.*/
   unlockSpinLockRestoreInterrupt(&ring->wait.lock,&rflags);
   return 0;
interrupted:
   unlockSpinLockRestoreInterrupt(&ring->wait.lock,&rflags);
         /*We are interrupted by signals......*/
   return -EINTR;
}
