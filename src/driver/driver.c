#include <core/const.h>
#include <driver/driver.h>
#include <core/list.h>
#include <cpu/spinlock.h>
#include <video/console.h>

static ListHead drivers = {};
static ListHead devices = {};

static SpinLock driverLock = {};
/*It's a spin lock of drivers and devices.*/

int registerDevice(Device *device)
{
   device->driver = 0;

   lockSpinLock(&driverLock);
   listAddTail(&device->list,&devices);

   for(ListHead *list = drivers.next;list != &drivers;list = list->next)
   { /*Foreach this list.*/
      Driver *driver = listEntry(list,Driver,list);
      if(driver->probe(device) == 0) 
      {
         if(driver->enable(device))
	    continue; /*Continue if error.*/
	 device->driver = driver;
         unlockSpinLock(&driverLock);
	 return 0;
      }
   }
   unlockSpinLock(&driverLock);
   return 1; 
      /*We didn't found a driver that can work with this device.*/
}

int deregisterDevice(Device *device)
{
   lockSpinLock(&driverLock);
   if(device->driver)
   {
      device->driver->disable(device);
         /*Should disable it first.*/
      device->driver = 0;
   }
   listDelete(&device->list);
   unlockSpinLock(&driverLock);
   return 0;
}

int registerDriver(Driver *driver)
{
   lockSpinLock(&driverLock);
   listAddTail(&driver->list,&drivers);

   for(ListHead *list = devices.next;list != &devices;list = list->next)
   {
      Device *device = listEntry(list,Device,list);
      if((!device->driver) && (driver->probe(device) == 0)) 
         driver->enable(device); /*Enable it.*/
   }
   unlockSpinLock(&driverLock);
   return 0;
}

int deregisterDriver(Driver *driver)
{
   lockSpinLock(&driverLock);
   for(ListHead *list = devices.next;list != &devices;list = list->next)
   {
      Device *device = listEntry(list,Device,list);
      if(device->driver == driver)
         driver->disable(device);
      device->driver = 0;
   } /*Fist foreach devices and disable the device that is using this driver.*/

   listDelete(&driver->list);
   unlockSpinLock(&driverLock);

   return 0;
}

int initDriver(void)
{
   initSpinLock(&driverLock);
   initList(&drivers);
   initList(&devices);
/*Init some lists and spin locks.*/
   return 0;
}
