#pragma once
#include <core/const.h>
#include <cpu/spinlock_types.h>
#include <core/list.h>

typedef struct Driver Driver;

typedef enum DeviceType{
   InvalidDevice,
   DeviceTypePCI
} DeviceType;

typedef struct Device{
   SpinLock lock;
   ListHead list;

   DeviceType type;

   Driver *driver;
} Device;

typedef struct Driver{
   int (*enable)(Device *device);
   int (*disable)(Device *device);
   int (*probe)(Device *device);

   ListHead list;
} Driver;


int initDriver(void);
int deregisterDriver(Driver *driver);
int registerDriver(Driver *driver);
int deregisterDevice(Device *device);
int registerDevice(Device *device);
