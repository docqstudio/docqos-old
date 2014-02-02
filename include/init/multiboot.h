#pragma once
#include <core/const.h>

/*See also http://nongnu.askapache.com/grub/phcoder/multiboot.pdf .*/

typedef struct MultibootTag{
   u32 type;
   u32 size;
} __attribute__ ((packed)) MultibootTag;

typedef struct MultibootColor{
   u8 red;
   u8 green;
   u8 blue;
} __attribute__ ((packed)) MultibootColor;

typedef struct MultibootTagFrameBuffer {
   MultibootTag tag;

   u64 address;
   u32 pitch;
   u32 width;
   u32 height;
   u8 bpp;
   u8 type;
   u16 reserved;

   union{
      struct{
         u8 redFieldPosition;
         u8 redMaskSize;
         u8 greenFieldPosition;
         u8 greenMaskSize;
         u8 blueFieldPosition;
         u8 blueMaskSize;
      } __attribute__ ((packed));
      struct{
         u16 count;
         MultibootColor palette[0];
      } __attribute__ ((packed));
   };
} __attribute__ ((packed)) MultibootTagFrameBuffer;

typedef struct MultibootMemoryMapEnrty{
   u64 address;
   u64 length;
   u32 type;
   u32 reserved;
} __attribute__ ((packed)) MultibootMemoryMapEntry;

typedef struct MultibootTagMemoryMap{
   MultibootTag tag;

   u32 count;
   u32 version;
   MultibootMemoryMapEntry entries[0];
} __attribute__ ((packed)) MultibootTagMemoryMap;

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

#define MULTIBOOT2_TAG_ALIGN                8
#define MULTIBOOT2_TAG_TYPE_END             0
#define MULTIBOOT2_TAG_TYPE_MEMORYMAP       6
#define MULTIBOOT2_TAG_TYPE_FRAMEBUFFER     8

