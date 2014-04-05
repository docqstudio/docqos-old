#include <core/const.h>
#include <task/waitqueue.h>

typedef struct RingBuffer
{
   unsigned int size;
       /*In fact,now it is always 4096.*/
   unsigned int reader; /*The reading position.*/
   unsigned int writer; /*The writing position.*/
   unsigned char buffer[4096]; /*The buffer.*/
   WaitQueue wait; /*The wait queue for inRingBuffer.*/
} RingBuffer;

inline int initRingBuffer(RingBuffer *ring) __attribute__ ((always_inline));
     /*Init a ring buffer.*/

inline int initRingBuffer(RingBuffer *ring)
{
   ring->reader = ring->writer = 0;
   ring->size = sizeof(ring->buffer);
   initWaitQueueHead(&ring->wait);
   return 0;
}

int outRingBuffer(RingBuffer *ring,unsigned int c);
int inRingBuffer(RingBuffer *ring,unsigned int *c);
    /*NOTE: The function can be interrupted by signals.*/
    /*The function will return -EINTR if that happens.*/
