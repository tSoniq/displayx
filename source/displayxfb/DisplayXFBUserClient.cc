/** @file   DisplayXFBUserClient.cc
 *  @brief  IOUserClient implementation for the virtual display.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 *
 *  The user-client provides the kernel<->user-mode communication. Several clients may be connected
 *  concurrently (limited by com_tsoniq_driver_DisplayXFBDriver::kMaxClients), and a state-less protocol is used
 *  for communication.
 */

#include "DisplayXFBUserClient.h"
#include "DisplayXFBDriver.h"
#include <IOKit/IOKitKeys.h>


using namespace ts;


// Console logging macro.
#if 0
#define TSLog(fmt, args...) do { IOLog("UC%p: %s: " fmt "\n", this, __PRETTY_FUNCTION__, ## args); } while (false)
#define TSTrace()           do { IOLog("UC%p: %s\n", this, __PRETTY_FUNCTION__); }while (false)
#else
#define TSLog(args...)      do { } while (false)
#define TSTrace()           do { } while (false)
#endif



#pragma mark    -
#pragma mark    OSObject Things



#define super IOUserClient
OSDefineMetaClassAndStructors(DisplayXFBUserClient, IOUserClient);




#pragma mark    -
#pragma mark    DisplayXFBUserClient Dispatch Table Handlers




/* Selector methods. These just bridge in to the correspondingly named class methods.
 */
IOReturn DisplayXFBUserClient::selectorUserClientOpen(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientOpen((DisplayXFBInfo*)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn DisplayXFBUserClient::selectorUserClientClose(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    (void)arguments;
    return target->userClientClose();
}

IOReturn DisplayXFBUserClient::selectorUserClientGetState(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientGetState((unsigned)arguments->scalarInput[0], (DisplayXFBState*)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn DisplayXFBUserClient::selectorUserClientGetConfiguration(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientGetConfiguration((unsigned)arguments->scalarInput[0], (DisplayXFBConfiguration*)arguments->structureOutput, &arguments->structureOutputSize);
}

IOReturn DisplayXFBUserClient::selectorUserClientSetConfiguration(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientSetConfiguration((unsigned)arguments->scalarInput[0], (DisplayXFBConfiguration*)arguments->structureInput, &arguments->structureInputSize);
}

IOReturn DisplayXFBUserClient::selectorUserClientConnect(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientConnect((unsigned)arguments->scalarInput[0]);
}

IOReturn DisplayXFBUserClient::selectorUserClientDisconnect(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientDisconnect((unsigned)arguments->scalarInput[0]);
}

IOReturn DisplayXFBUserClient::selectorUserClientMap(DisplayXFBUserClient* target, void* reference, IOExternalMethodArguments* arguments)
{
    (void)reference;
    return target->userClientMap((unsigned)arguments->scalarInput[0], (unsigned)arguments->scalarInput[1], (bool)arguments->scalarInput[2], (DisplayXFBMap*)arguments->structureOutput, &arguments->structureOutputSize);
}



/** The selector dispatch table (supports 10.5 or later only).
 *
 *  @warning    The order of declarations must match the selector index values in DisplayXFBShared.h
 */
const IOExternalMethodDispatch DisplayXFBUserClient::selectorMethods[kDisplayXFBNumberSelectors] =
{
    {   // kDisplayXFBSelectorOpen
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientOpen,
        0,                                      // No scalar input values.
        0,                                      // No struct input value.
        0,                                      // No scalar output values.
        sizeof(DisplayXFBInfo)                  // Size of output structure (the driver info structure)
    },
    {   // kDisplayXFBSelectorClose
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientClose,
        0,                                      // No scalar input values.
        0,                                      // No struct input value.
        0,                                      // No scalar output values.
        0                                       // No struct output value.
    },
    {   // kDisplayXFBSelectorGetState
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientGetState,
        1,                                      // Number of scalar inputs {displayIndex}
        0,                                      // Size of input structure
        0,                                      // Number of scalar outputs
        sizeof(DisplayXFBState)                // Size of output structure (the display state)
    },
    {   // kDisplayXFBSelectorGetConfiguration
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientGetConfiguration,
        1,                                      // Number of scalar inputs {displayIndex}
        0,                                      // Size of input structure
        0,                                      // Number of scalar outputs
        sizeof(DisplayXFBConfiguration)         // Size of output structure (the display configuration)
    },
    {   // kDisplayXSelectorSetConfiguration
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientSetConfiguration,
        1,                                      // Number of scalar inputs {displayIndex}
        sizeof(DisplayXFBConfiguration),        // Size of input structure
        0,                                      // Number of scalar outputs
        0                                       // Size of output structure
    },
    {   // kDisplayXFBSelectorConnect
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientConnect,
        1,                                      // Number of scalar inputs {displayIndex}
        0,                                      // Size of input structure
        0,                                      // Number of scalar outputs
        0                                       // Size of output structure
    },
    {   // kDisplayXSelectorDisconnect
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientDisconnect,
        1,                                      // Number of scalar inputs {displayIndex}
        0,                                      // Size of input structure
        0,                                      // Number of scalar outputs
        0                                       // Size of output structure
    },
    {   // kDisplayXFBSelectorMap
        (IOExternalMethodAction)&DisplayXFBUserClient::selectorUserClientMap,
        3,                                      // Number of scalar inputs {displayIndex, mapType, readOnly}
        0,                                      // Size of input structure
        0,                                      // Number of scalar outputs
        sizeof(DisplayXFBMap)                   // Size of output structure (the memory mapping)
    }
};



/** The selector method dispatch.
 */
IOReturn DisplayXFBUserClient::externalMethod(
    uint32_t selector,
    IOExternalMethodArguments* arguments,
    IOExternalMethodDispatch* dispatch,
    OSObject* target,
    void* reference)
{
    TSLog("sel %u", (unsigned)selector);

    if (selector < (uint32_t)(sizeof selectorMethods / sizeof selectorMethods[0]))
    {
        dispatch = (IOExternalMethodDispatch*)&selectorMethods[selector];
        if (!target) target = this;
    }

    return super::externalMethod(selector, arguments, dispatch, target, reference);
}




#pragma mark    -
#pragma mark    IOUserClient




/** Initialisation.
 *  Called as a result of the user process calling IOServiceOpen().
 */
bool DisplayXFBUserClient::initWithTask(task_t owningTask, void* securityToken, UInt32 type, OSDictionary* properties)
{
    TSTrace();

    m_provider = 0;
    m_owningTask = 0;

    for (unsigned i = 0; i < ts::kDisplayXFBMaxDisplays; i++)
    {
        for (unsigned j = 0; j != kDisplayXFBMaxMapTypes; j++)
        {
            m_memoryMaps[i][j] = 0;
        }
    }

   bool ok = false;
    if (properties && properties->getObject(kIOUserClientCrossEndianKey))
    {
        // A connection to this user client is being opened by a user process that needs endian flip nonsense.
        // This will only happen on an Intel 10.5 machine which is running a client in Rosetta (PPC emulation).
        // We don't support this, as it would require endian mapping of all control and video stream data.
        IOLog("%s: request to open driver from Rosetta rejected\n", __PRETTY_FUNCTION__);
    }
    else
    {
        ok = super::initWithTask(owningTask, securityToken, type, properties);
        if (!ok) IOLog("%s: superclass failed initialisation", __PRETTY_FUNCTION__);
        else
        {
            // Successful initialisation.
            m_owningTask = owningTask;
        }
    }
    return ok;
}


/** Object start.
 *  Called after initWithTask() as a result of the user process calling IOServiceOpen.
 */
bool DisplayXFBUserClient::start(IOService* provider)
{
    TSLog("provider %p", provider);

    bool ok = false;

    com_tsoniq_driver_DisplayXFBDriver* xprovider = OSDynamicCast(com_tsoniq_driver_DisplayXFBDriver, provider);

    if (xprovider && super::start(provider))
    {
        ok = true;
        m_provider = xprovider;
        m_provider->retain();
    }

    return ok;
}


/** Object stop.
 */
void DisplayXFBUserClient::stop(IOService* provider)
{
    TSLog("provider %p", provider);

    super::stop(provider);
    if (m_provider)
    {
        m_provider->release();
        m_provider = 0;
    }
}


/** Called as a result of the client calling IOServiceClose().
 */
IOReturn DisplayXFBUserClient::clientClose()
{
    // Defensive coding in case the user process called IOServiceClose
    // without calling closeUserClient first.
    userClientClose();

    // Inform the user process that this user client is no longer available. This will also cause the
    // user client instance to be destroyed.
    //
    // terminate would return false if the user process still had this user client open.
    // This should never happen in our case because this code path is only reached if the user process
    // explicitly requests closing the connection to the user client.
    bool success = terminate();
    if (!success) IOLog("%s[%p]::%s(): terminate() failed.\n", getName(), this, __FUNCTION__);


    // DON'T call super::clientClose, which just returns kIOReturnUnsupported.
    return kIOReturnSuccess;
}


/** Called as a result of the client user process unexpectedly exiting (ie crashing).
 */
IOReturn DisplayXFBUserClient::clientDied()
{
    IOReturn result = super::clientDied();
    return result;
}


/** Notification that termination is starting.
 *  It is a notification that a provider has been terminated, sent before recursing up the stack, in root-to-leaf order.
 *  This is where any pending I/O should be terminated. At this point the user client has been marked inactive and any
 *  further requests from the user process should be returned with an error.
 */
bool DisplayXFBUserClient::willTerminate(IOService* provider, IOOptionBits options)
{
    return super::willTerminate(provider, options);
}


/** Notification that termination has completed.
 */
bool DisplayXFBUserClient::didTerminate(IOService* provider, IOOptionBits options, bool* defer)
{
    // If all pending I/O has been terminated, close our provider. If I/O is still outstanding, set defer to true
    // and the user client will not have stop called on it.
    userClientClose();
    *defer = false;
    return super::didTerminate(provider, options, defer);
}


/** Finalisation notification. Unused at present.
 */
bool DisplayXFBUserClient::finalize(IOOptionBits options)
{
    bool ok = super::finalize(options);
    return ok;
}



#pragma mark    -
#pragma mark    DisplayXFBUserClient Methods


/** This gets called in response to the driver (our provider) calling messageClients() with a suitable code.
 *  The default implementation of this just returns 'not supported', so we have to override the default
 *  method with one that will forward the message to the actual user code (our client).
 */
IOReturn DisplayXFBUserClient::message(UInt32 type, IOService* provider, void* argument)
{
    /* From debugging, we seem to see several non-DisplayX messages received here:
     *
     *      Type code   Symbol
     *      e0000101    kIOMessageServiceIsAttemptingOpen   Seen at power-on, when the driver first loads
     *      e0000210    kIOMessageDeviceWillPowerOff        Seen before system sleep
     *      e0000230    kIOMessageDeviceHasPoweredOn        Seen after system wake
     *
     *  See IOMessage.h for some definitions.
     */

    IOReturn status;

    switch (type)
    {
        case kDisplayXFBNotificationDisplayState:
        case kDisplayXFBNotificationCursorState:
        case kDisplayXFBNotificationCursorImage:
            status = messageClients(type, argument);
            break;

        default:
            status = super::message(type, provider, argument);
            break;
    }

    return status;
}





#pragma mark    -
#pragma mark    DisplayXFBUserClient Methods





/** Request from a client to open a session.
 *
 *  @param  info        A pointer to the info structure to receive the driver information on successful completion.
 *  @param  infoSize    On entry, the client size allocation for the info structure. On return the actual size.
 *  @return             The completion status.
 *                      kIOReturnBadArgument - Invalid caller argument (random 3rd party hackery or a bug)
 *                      kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                      kIOReturnError - Provider returned an error (likely, but not necessarily, too many concurrent clients)
 *                      kIOReturnSuccess - Client has been closed.
 */
IOReturn DisplayXFBUserClient::userClientOpen(DisplayXFBInfo* info, uint32_t* infoSize)
{
    //IOLog("%s[%p]::%s: info %p/%u\n", getName(), this, __FUNCTION__, info, (unsigned)*infoSize);

    IOReturn status;
    if (!info || !infoSize || *infoSize != sizeof *info)
    {
        //IOLog("%s[%p]::%s: Invalid arguments\n", getName(), this, __FUNCTION__);
        status = kIOReturnBadArgument;
    }
    else if (!m_provider || isInactive())
    {
        // Return an error if we don't have a provider. This could happen if the user process
        // called openUserClient without calling IOServiceOpen first. Or, the user client could be
        // in the process of being terminated and is thus inactive.
        //IOLog("%s[%p]::%s: not attached\n", getName(), this, __FUNCTION__);
        status = kIOReturnNotAttached;
    }
    else if (!m_provider->open(this))
    {
        // The open may fail if too many clients are currently attached.
        //IOLog("%s[%p]::%s: provider open failed\n", getName(), this, __FUNCTION__);
        status = kIOReturnError;
    }
    else
    {
        status = m_provider->userClientOpen(info);
        if (kIOReturnSuccess != status)
        {
            //IOLog("%s[%p]::%s: frame buffer open failed with %08x\n", getName(), this, __FUNCTION__, (unsigned)status);
            m_provider->close(this);
        }
    }

    if (kIOReturnSuccess != status && infoSize) *infoSize = 0;      // If returning an error, also signal that no data is returned
    return status;
}


/** Request from a client to close a session.
 *
 *  @return             The completion status.
 *                      kIOReturnNotOpen - Client was not open.
 *                      kIOReturnSuccess - Client has been closed.
 */
IOReturn DisplayXFBUserClient::userClientClose()
{
    //IOLog("%s[%p]::%s\n", getName(), this, __FUNCTION__);
    IOReturn status;
    if (!m_provider || !m_provider->isOpen(this))
    {
        //IOLog("%s[%p]::%s: not open\n", getName(), this, __FUNCTION__);
        status = kIOReturnNotOpen;
    }
    else
    {
        // Clean up any memory mappings
        for (unsigned i = 0; i < ts::kDisplayXFBMaxDisplays; i++)
        {
            for (unsigned j = 0; j != kDisplayXFBMaxMapTypes; j++)
            {
                IOMemoryMap* map = m_memoryMaps[i][j];
                if (map)
                {
                    map->release();
                    m_memoryMaps[i][j] = 0;
                }
            }
        }

        // Close the device.
        m_provider->userClientClose();

        // Close the provider.
        m_provider->close(this);
        status = kIOReturnSuccess;
    }

    return status;
}


/** Return a display's current state.
 *
 *  @param  index           The display index (0 - n-1).
 *  @param  state           Structure to receive the display information.
 *  @param  stateSize       On entry, the display structure allocation size, on return the size written.
 *  @return                 The completion status.
 *                          kIOReturnBadArgument - supplied parameters are unusable (eg incorrect structure size).
 *                          kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                          kIOReturnNotOpen - client is not open.
 *                          kIOReturnNotFound - the specified display does not exist.
 *                          kIOReturnError - the display could not be queried.
 *                          kIOReturnSuccess - display info was returned.
 */
IOReturn DisplayXFBUserClient::userClientGetState(unsigned index, DisplayXFBState* state, uint32_t* stateSize)
{
    //IOLog("%s[%p]::%s: state %p/%u\n", getName(), this, __FUNCTION__, state, (unsigned)*stateSize);

    IOReturn status;

    if (!state || !stateSize || *stateSize != sizeof *state)            status = kIOReturnBadArgument;
    else if (!m_provider || isInactive())                               status = kIOReturnNotAttached;
    else if (!m_provider->isOpen(this))                                 status = kIOReturnNotOpen;
    else if (!m_provider->validateDisplayIndex(index))                  status = kIOReturnNotFound;
    else status = m_provider->userClientGetState(state, index);

    if (kIOReturnSuccess != status && stateSize) *stateSize = 0;  // If returning an error, also signal that no data is returned
    return status;
}


/** Return a display's current configuration.
 *
 *  @param  index           The display index (0 - n-1).
 *  @param  config          Structure to receive the display information.
 *  @param  configSize      On entry, the display structure allocation size, on return the size written.
 *  @return                 The completion status.
 *                          kIOReturnBadArgument - supplied parameters are unusable (eg incorrect structure size).
 *                          kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                          kIOReturnNotOpen - client is not open.
 *                          kIOReturnNotFound - the specified display does not exist.
 *                          kIOReturnError - the display could not be queried.
 *                          kIOReturnSuccess - display info was returned.
 */
IOReturn DisplayXFBUserClient::userClientGetConfiguration(unsigned index, DisplayXFBConfiguration* config, uint32_t* configSize)
{
    //IOLog("%s[%p]::%s: config %p/%u\n", getName(), this, __FUNCTION__, config, (unsigned)*configSize);

    IOReturn status;

    if (!config || !configSize || *configSize != sizeof *config)        status = kIOReturnBadArgument;
    else if (!m_provider || isInactive())                               status = kIOReturnNotAttached;
    else if (!m_provider->isOpen(this))                                 status = kIOReturnNotOpen;
    else if (!m_provider->validateDisplayIndex(index))                  status = kIOReturnNotFound;
    else status = m_provider->userClientGetConfiguration(config, index);

    if (kIOReturnSuccess != status && configSize) *configSize = 0;  // If returning an error, also signal that no data is returned
    return status;
}


/** Set a display's current configuration.
 *
 *  @param  displayIndex    The display to configure.
 *  @param  config          The configuration to apply.
 *  @param  configSize      The size of the configuration structure.
 *  @return                 The completion status.
 *                          kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                          kIOReturnNotOpen - client is not open.
 *                          kIOReturnNotFound - the specified display does not exist.
 *                          kIOReturnError - the display could not be connected.
 *                          kIOReturnSuccess - display info was returned.
 */
IOReturn DisplayXFBUserClient::userClientSetConfiguration(unsigned displayIndex, DisplayXFBConfiguration* config, uint32_t* configSize)
{
    //IOLog("%s[%p]::%s: displayIndex %u\n", getName(), this, __FUNCTION__, displayIndex);

    IOReturn status;

    if (!config || !configSize || *configSize != sizeof *config)            status = kIOReturnBadArgument;
    else if (!config->isValid())                                            status = kIOReturnBadArgument;
    else if (!m_provider || isInactive())                                   status = kIOReturnNotAttached;
    else if (!m_provider->isOpen(this))                                     status = kIOReturnNotOpen;
    else if (!m_provider->validateDisplayIndex(displayIndex))               status = kIOReturnNotFound;
    else status = m_provider->userClientSetConfiguration(config, displayIndex);

    return status;
}


/** Try to connect a display.
 *
 *  @param  displayIndex    The display to configure.
 *  @return                 The completion status.
 *                          kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                          kIOReturnNotOpen - client is not open.
 *                          kIOReturnNotFound - the specified display does not exist.
 *                          kIOReturnError - the display could not be connected.
 *                          kIOReturnSuccess - display info was returned.
 */
IOReturn DisplayXFBUserClient::userClientConnect(unsigned displayIndex)
{
    //IOLog("%s[%p]::%s: displayIndex %u\n", getName(), this, __FUNCTION__, displayIndex);

    IOReturn status;

    if (!m_provider || isInactive())                                    status = kIOReturnNotAttached;
    else if (!m_provider->isOpen(this))                                 status = kIOReturnNotOpen;
    else status = m_provider->userClientConnect(displayIndex);

    return status;
}


/** Disconnect a display.
 *
 *  @param  displayIndex    The display to configure.
 *  @return                 The completion status.
 *                          kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                          kIOReturnNotOpen - client is not open.
 *                          kIOReturnNotFound - the specified display does not exist.
 *                          kIOReturnError - the display could not be connected.
 *                          kIOReturnSuccess - display info was returned.
 */
IOReturn DisplayXFBUserClient::userClientDisconnect(unsigned displayIndex)
{
    //IOLog("%s[%p]::%s: displayIndex %u\n", getName(), this, __FUNCTION__, displayIndex);

    IOReturn status;

    if (!m_provider || isInactive())                                    status = kIOReturnNotAttached;
    else if (!m_provider->isOpen(this))                                 status = kIOReturnNotOpen;
    else status = m_provider->userClientDisconnect(displayIndex);

    return status;
}


/** Map shared data in to a task's address space.
 *
 *  The shared data address and allocation size must remain the constant and valid regardless of display
 *  connect/disconnect. The mapping is released only when the client task closes the connection.
 *
 *  @param  displayIndex    The display index (0 - n-1).
 *  @param  mapType         The object type to be mapped (kDisplayXFBMapTypeXyz).
 *  @param  readOnly        Non-zero to map the data read-only (normally, only the GA will require write-access).
 *  @param  map             Structure to receive the display information.
 *  @param  mapSize         On entry, the display structure allocation size, on return the size written.
 *  @return                 The completion status.
 *                          kIOReturnBadArgument - supplied parameters are unusable (eg incorrect structure size).
 *                          kIOReturnNotAttached - Client service is not correctly attached (likely client bug)
 *                          kIOReturnNotOpen - client is not open.
 *                          kIOReturnNotFound - the specified display does not exist.
 *                          kIOReturnNoMemory - the mapping could not be established.
 *                          kIOReturnSuccess - display info was returned.
 */
IOReturn DisplayXFBUserClient::userClientMap(unsigned displayIndex, unsigned mapType, bool readOnly, DisplayXFBMap* map, uint32_t* mapSize)
{
    TSLog("displayIndex %u %u %d", displayIndex, mapType, (int)readOnly);

    IOReturn status;

    if (!map || !mapSize || *mapSize != sizeof *map)            status = kIOReturnBadArgument;
    else if (mapType >= kDisplayXFBMaxMapTypes)                 status = kIOReturnBadArgument;
    else if (!m_provider || isInactive())                       status = kIOReturnNotAttached;
    else if (!m_provider->isOpen(this))                         status = kIOReturnNotOpen;
    else if (!m_provider->validateDisplayIndex(displayIndex))   status = kIOReturnNotFound;
    else
    {
        IOMemoryMap* iomap = m_memoryMaps[displayIndex][mapType];
        if (!iomap) iomap = m_provider->userClientMapInTask((readOnly != 0) ? true : false, m_owningTask, displayIndex, mapType);
        if (!iomap)
        {
            status = kIOReturnNoMemory;
        }
        else
        {
            // We have a valid mapping.
            // Note: documentation says to use getVirtualAddress, but internet & console logs say to use
            // the undocumented getAddress() for 32/64 bit compatibility.
            m_memoryMaps[displayIndex][mapType] = iomap;
            map->initialise(iomap->getAddress(), iomap->getLength());
            TSLog("Map --> %p, %u", (void*)iomap->getAddress(), (unsigned)iomap->getLength());
            status = kIOReturnSuccess;
        }
    }

    if (kIOReturnSuccess != status && mapSize) *mapSize = 0;    // If returning an error, also signal that no data is returned
    return status;
}
