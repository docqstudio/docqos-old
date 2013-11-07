#include <lib/string.h>
#include <core/const.h>

void *memcpy(void *_to,const void *_from,int n) /*NOTE: It's easy enough,but it's also slow enough.*/
{
   u8 *to = (u8 *)_to;
   const u8 *from = (const u8 *)_from;
   for(int i = 0;i < n;++i){
      *(to++) = *(from++);
   }
   return _to;
}
