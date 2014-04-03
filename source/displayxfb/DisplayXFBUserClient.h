/** @file   DisplayXFBUserClient.h
 *  @brief  IOUserClient implementation for the virtual display.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_DisplayXFBUserClient_H
#define COM_TSONIQ_DisplayXFBUserClient_H   (1)

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include "DisplayXFBNames.h"
#include "DisplayXFBDriver.h"
#include "DisplayXFBShared.h"

class DisplayXFBUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(DisplayXFBUserClient);

public:

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* arguments, IOExternalMethodDispatch* dispatch, OSObject* target, void* reference);
    virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties);
    virtual bool start(IOService* provider);
    virtual void stop(IOService* provider);
    virtual IOReturn clientClose();
    virtual IOReturn clientDied();
    virtual bool willTerminate(IOService* provider, IOOptionBits options);
    virtual bool didTerminate(IOService* provider, IOOptionBits options, bool* defer);
    virtual bool finalize(IOOptionBits options);

    // Messaging
    virtual IOReturn message(UInt32 type, IOService* provider, void* argument);


    // User client methods
    IOReturn userClientOpen(ts::DisplayXFBInfo* info, uint32_t* infoSize);
    IOReturn userClientClose();
    IOReturn userClientGetConfiguration(uint32_t displayIndex, ts::DisplayXFBConfiguration* config, uint32_t* configSize);
    IOReturn userClientSetConfiguration(uint32_t displayIndex, ts::DisplayXFBConfiguration* config, uint32_t* configSize);
    IOReturn userClientGetState(uint32_t displayIndex, ts::DisplayXFBState* state, uint32_t* stateSize);
    IOReturn userClientConnect(unsigned displayIndex);
    IOReturn userClientDisconnect(unsigned displayIndex);
    IOReturn userClientMap(unsigned displayIndex, unsigned mapType, bool readOnly, ts::DisplayXFBMap* map, uint32_t* mapSize);

    static const IOExternalMethodDispatch selectorMethods[ts::kDisplayXFBNumberSelectors];

    static IOReturn selectorUserClientOpen(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientClose(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientGetConfiguration(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientSetConfiguration(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientGetState(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientConnect(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientDisconnect(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);
    static IOReturn selectorUserClientMap(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments);

private:

    com_tsoniq_driver_DisplayXFBDriver* m_provider;                                     //! The providing service
    task_t m_owningTask;                                                                //! The client's task handle
    IOMemoryMap* m_memoryMaps[ts::kDisplayXFBMaxDisplays][kDisplayXFBMaxMapTypes];      //! Array of memory mappings

    DisplayXFBUserClient(const DisplayXFBUserClient&);              // Prevent copy constructor
    DisplayXFBUserClient& operator=(const DisplayXFBUserClient&);   // Prevent assignment
};


#endif      // COM_TSONIQ_DisplayXFBUserClient_H
