/** @file   DisplayXFBDriver.cc
 *  @brief  The driver object to which the user-client is bound.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#include "DisplayXFBDriver.h"
#include "DisplayXFBFramebuffer.h"
#include "DisplayXFBPowerState.h"

using namespace ts;

#pragma mark    -
#pragma mark    Miscellaneous Support


// Console logging macro.
#if 0
#define TSLog(fmt, args...) do { IOLog("DR%p: %s: " fmt "\n", this, __PRETTY_FUNCTION__, ## args); } while (false)
#define TSTrace()           do { IOLog("DR%p: %s\n", this, __PRETTY_FUNCTION__); }while (false)
#else
#define TSLog(args...)      do { } while (false)
#define TSTrace()           do { } while (false)
#endif




#pragma mark    -
#pragma mark    DisplayXFBDriver  (IOService methods)




#define super IOService
OSDefineMetaClassAndStructors(DisplayXFBDriver, IOService);


/** Demo expiry date (as seconds since 1st Jan 1970).
 *  See http://unixtime-converter.com for an online converter.
 */
#ifndef DISPLAYX_EXPIRY_EPOCH
//#define DISPLAYX_EXPIRY_EPOCH   (1320105599u + 273u)    // 2011-10-31 23:59:59 + a small offset for mysteriosity
#endif


/** Object initialisation.
 */
bool DisplayXFBDriver::init(OSDictionary* dictionary)
{
    TSTrace();

    // Initialise local variables with default (constructor).
    m_displayCount = 0;
    m_vramSize = 0;
    memset(m_displayName, 0, sizeof m_displayName);
    m_accelerator = 0;
    for (unsigned i = 0; i < sizeof m_framebuffers / sizeof m_framebuffers[0]; i++) m_framebuffers[i] = 0;
    for (unsigned i = 0; i < sizeof m_clients / sizeof m_clients[0]; i++) m_clients[i] = 0;


    // Validate expiry date. Very crude protection against permanent use.
#ifdef DISPLAYX_EXPIRY_EPOCH
    clock_sec_t secs = 0;
    clock_usec_t microsecs = 0;
    clock_get_calendar_microtime(&secs, &microsecs);
    if (secs >= DISPLAYX_EXPIRY_EPOCH)
    {
        IOLog("The DisplayX driver %u.%u evaluation period has expired. Contact support@tsoniq for information.\n",
            (unsigned)DisplayXFBInfo::kVersionMajor, (unsigned)DisplayXFBInfo::kVersionMinor);
        return false;
    }
    else
    {
        const unsigned days = (unsigned)((DISPLAYX_EXPIRY_EPOCH - secs) / 86400);
        IOLog("DisplayX driver %u.%u evaluation. %u days remaining. Copyright (c) 2010-2014 tSoniq. All Rights Reserved.\n",
            (unsigned)DisplayXFBInfo::kVersionMajor, (unsigned)DisplayXFBInfo::kVersionMinor, days);
    }
#endif


    // Perform superclass initialisation.
    if (!super::init(dictionary)) return false;


    // Read the configuration to use from the plist entries.
    getPropertyU32(&m_displayCount, kDisplayXFBKeyDisplayCount, 1, kDisplayXFBMaxDisplays, 1);
    getPropertyU32(&m_vramSize, kDisplayXFBKeyVRAMSize, kDisplayXFBMinVRAMSize, kDisplayXFBMaxVRAMSize, kDisplayXFBDefaultVRAMSize);
    getPropertyStr(m_displayName, sizeof m_displayName, kDisplayXFBKeyDisplayName, "DisplayX");

    // Round the VRAM size to the next higher number of MBytes.
    m_vramSize = ((m_vramSize + 0xfffff) >> 20) << 20;

    return true;
}


/** Object release.
 */
void DisplayXFBDriver::free()
{
    TSTrace();

    for (unsigned i = 0; i < sizeof m_clients / sizeof m_clients[0]; i++)
    {
        if (m_clients[i] != 0) { IOLog("Driver being freed with open client(s)\n"); break; }
    }

    super::free();
}


/** Probe request.
 */
IOService* DisplayXFBDriver::probe(IOService* provider, SInt32* score)
{
    TSTrace();
    IOService* service = super::probe(provider, score);
    return service;
}


/** Start the driver.
 *
 *  The driver is loaded automatically at boot-time. At this point we can parse the info.plist
 *  settings to determine our configuration and create any framebuffer objects as required.
 *
 *  @param  provider    The provider service.
 *  @return             Logical true for success, false if we are unable to start.
 */
bool DisplayXFBDriver::start(IOService* provider)
{
    TSTrace();

    // Initialise the provider.
    if (!super::start(provider)) return false;


    if (m_displayCount > sizeof m_framebuffers / sizeof m_framebuffers[0])
    {
        TSLog("Broken display count - got %u but max %u", m_displayCount, (unsigned)(sizeof m_framebuffers / sizeof m_framebuffers[0]));
        return false;
    }

    // Create the accelerator.
    m_accelerator = OSTypeAlloc(com_tsoniq_driver_DisplayXFBAccelerator);
    if (!m_accelerator)
    {
        TSLog("Failed to create accelerator");
    }
    else
    {
        // Must attach before calling start() (so that start can see the registry definitions)
        if (!m_accelerator->init(0) || !m_accelerator->attach(this) || !m_accelerator->start(this))
        {
            TSLog("Failed to initialise/start accelerator");
            m_accelerator->release();
            m_accelerator = 0;
        }
    }


    // Create nubs for each requested display. Each nub is an instance of an IOFramebuffer object.
    for (unsigned index = 0; index < m_displayCount; index++)
    {
        com_tsoniq_driver_DisplayXFBFramebuffer* nub = OSTypeAlloc(com_tsoniq_driver_DisplayXFBFramebuffer);
        if (!nub) { TSLog("Failed to create driver nub"); continue; }
        TSLog("Create framebuffer nub @ %p", nub);

        if (!nub->init(0))
        {
            TSLog("Failed to initialise nub");
            nub->release();
            continue;
        }

        //TSLog("Attaching nub @ %p", nub);
        if (!nub->attach(this))
        {
            TSLog("Failed to attach nub");
            nub->release();
            continue;
        }

        // Remember the device.
        m_framebuffers[index] = nub;

        // Create a display name
        char name[sizeof m_displayName + 16];
        if (index == 0) strncpy(name, m_displayName, sizeof name);          // First display - use base name only
        else snprintf(name, sizeof name, "%s %u", m_displayName, index+1);  // Additional displays - add index qualifier
        nub->setProperty(kDisplayXFBKeyDisplayName, name);

        // Set some keys on the nub (really should be done in the framebuffer, but the API provides no other means to do this).
        nub->setProperty(kDisplayXFBKeyDisplayIndex, index, 32);        // The display index number (as passed in interface methods)
        nub->setProperty(kDisplayXFBKeyVRAMSize, m_vramSize, 32);       // The VRAM size

        // Set the nub location (for IOKit search disambiguation)
        char location[16];
        snprintf(location, sizeof location, "%u", index);
        nub->setLocation(location);

        // Start the nub.
        if (!nub->start(this))
        {
            TSLog("Failed to start nub");
            nub->release();
            continue;
        }

        TSLog("Completed set up for nub %s @ %p", name, nub);
        nub->registerService();                                             // Start service matching for the nub

        // Do we need to release the nub here, as it is retained following register service? In any event,
        // display card drivers are never unloaded...
    }


    // Configure power management.
    TSLog("Config PM");
    PMinit();
    getProvider()->joinPMtree(this);
    registerPowerDriver(this, DisplayXFBDriverPowerStates, kDisplayXFBNumPowerStates);
    changePowerStateTo(kDisplayXFBPowerStateWake);


    // Allow applications to find us now that everything is ready.
    registerService();


    //TSLog("success");
    return true;
}


/** Stop the driver.
 */
void DisplayXFBDriver::stop(IOService* provider)
{
    TSTrace();

    TSLog("Stop PM");
    PMstop();

    // Release the attached nubs, created in start().
	OSIterator* iter = getClientIterator();
	while (true)
	{
	    IOService* client = (IOService*)iter->getNextObject();
	    if (!client) break;     // No more clients

		//TSLog("terminate client %p", client);
		client->terminate(kIOServiceRequired | kIOServiceTerminate | kIOServiceSynchronous);
		client->release();
	}
	iter->release();

    super::stop(provider);
}


/** Handle power state management.
 */
IOReturn DisplayXFBDriver::setPowerState(unsigned long whichState, IOService* whatDriver)
{
    (void)whichState;
    (void)whatDriver;
    TSLog("state %u", (unsigned)whichState);
    return IOPMAckImplied;
}


/** Handle open() requests. This is called in response to a new user-client invoking provider->open().
 */
bool DisplayXFBDriver::handleOpen(IOService* forClient, IOOptionBits options, void* arg)
{
    (void)options;
    (void)arg;

    if (0 == forClient)
    {
        TSLog("Open with null client denied");
        return false;
    }
    else if (handleIsOpen(forClient))
    {
        TSLog("Duplicate client-open request");
        return false;
    }
    else
    {
        TSLog("Open client");
        for (unsigned i = 0; i < sizeof m_clients / sizeof m_clients[0]; i++)
        {
            if (0 == m_clients[i])
            {
                m_clients[i] = forClient;
                return true;
            }
        }
        TSLog("Too many clients open");
        return false;
    }
}


/** Handle close() requests.
 */
void DisplayXFBDriver::handleClose(IOService* forClient, IOOptionBits options)
{
    TSTrace();
    (void)options;
    for (unsigned i = 0; i < sizeof m_clients / sizeof m_clients[0]; i++)
    {
        if (forClient == m_clients[i]) m_clients[i] = 0;
    }
}


/** Check if a session is open for a given client.
 */
bool DisplayXFBDriver::handleIsOpen(const IOService* forClient) const
{
    TSTrace();
    for (unsigned i = 0; i < sizeof m_clients / sizeof m_clients[0]; i++)
    {
        if (forClient == m_clients[i]) return true;
    }
    return false;
}




#pragma mark    -
#pragma mark    Local Helper Methods



/** Get the framebuffer for a specific display index number.
 *
 *  @param  displayIndex    The display number.
 *  @return                 A reference to the framebufer for the display, or zero if not found. The returned
 *                          nub is *not* retained and should not be released by the caller.
 */
class com_tsoniq_driver_DisplayXFBFramebuffer* DisplayXFBDriver::indexToFramebuffer(unsigned displayIndex) const
{
    return (displayIndex < sizeof m_framebuffers / sizeof m_framebuffers[0]) ? m_framebuffers[displayIndex] : 0;
}



/** Get the display index number for a given framebuffer.
 *
 *  @param  framebuffer     The framebuffer instance.
 *  @return                 The corresponding display index, or kDisplayXFBMaxClients if not found.
 */
unsigned DisplayXFBDriver::framebufferToIndex(const com_tsoniq_driver_DisplayXFBFramebuffer* framebuffer) const
{
    unsigned displayIndex = 0;
    while (displayIndex < kDisplayXFBMaxDisplays)
    {
        if (m_framebuffers[displayIndex] == framebuffer) break;
        displayIndex ++;
    }
    return displayIndex;
}




/** Helper function used to read property keys, checking for errors and applying range limiting.
 *
 *  @param  value           The value to assign.
 *  @param  key             The key.
 *  @param  minValue        The minimum permitted value. Inclusive.
 *  @param  maxValue        The maximum permitted value. Inclusive.
 *  @param  defValue        The default value to use if the key is not found.
 *  @return                 True for success, false if a default or limited value has been applied.
 */
bool DisplayXFBDriver::getPropertyU32(unsigned* value, const char* key, unsigned minValue, unsigned maxValue, unsigned defValue) const
{
    bool ok;
    unsigned result;

    OSNumber* number = OSDynamicCast(OSNumber, getProperty(key));
    if (number) { result = number->unsigned32BitValue(); ok = true; }
    else { result = defValue; ok = false; }

    if (result < minValue) { result = minValue; ok = false; }
    else if (result > maxValue) { result = maxValue; ok = false; }

    if (!ok) TSLog("Invalid property %s: applying value %u", key, result);
    *value = result;
    return ok;
}



/** Helper function used to read property keys, checking for errors and applying range limiting.
 *
 *  @param  str             The cString to receive the result.
 *  @param  size            The storage space for the string (including null terminator).
 *  @param  key             The key.
 *  @param  defValue        The default value to use if the key is not found.
 *  @return                 True for success, false if a default or limited value has been applied.
 */
bool DisplayXFBDriver::getPropertyStr(char* str, size_t size, const char* key, const char* defValue) const
{
    bool ok = false;

    if (size == 0)
    {
        // No space for anything.
    }
    else if (size == 1)
    {
        // Space only for a terminator
        str[0] = 0;
    }
    else
    {
        if (!defValue) defValue = "";

        OSString* osString = OSDynamicCast(OSString, getProperty(key));
        const char* cString = (osString) ? (osString->getCStringNoCopy()) : 0;
        if (cString) ok = true;
        else cString = defValue;

        memset(str, 0, size);
        strncpy(str, cString, size);
        str[size - 1] = 0;

        ok = ok && (strlen(str) == strlen(cString));
    }

    if (!ok) TSLog("Invalid property %s: applying value \"%s\"", key, (size != 0) ? str : "<null>");
    return ok;
}



#pragma mark    -
#pragma mark    User Client Methods




/** Open a new session.
 *
 *  @param  info        Returns the driver info structure.
 *  @return             An IOReturn code.
 *  @note               The user client will first open() this driver, resulting in the handleOpen() methods being
 *                      being run before this one.
 */
IOReturn DisplayXFBDriver::userClientOpen(DisplayXFBInfo* info)
{
    TSTrace();
    if (!info) return kIOReturnBadArgument;

    info->initialise(m_displayCount);
    return kIOReturnSuccess;
}


/** Close an existing session.
 *
 *  @return             An IOReturn code.
 *  @note               The user client will call close() after this call, regardless of the return status.
 */
IOReturn DisplayXFBDriver::userClientClose()
{
    TSTrace();
    return kIOReturnSuccess;
}


/** Query the current display information.
 *
 *  @param  config          Returns the display structure.
 *  @param  displayIndex    The display index.
 *  @return                 An IOReturn code.
 */
IOReturn DisplayXFBDriver::userClientGetConfiguration(DisplayXFBConfiguration* config, unsigned displayIndex)
{
    TSTrace();
    if (!config) return kIOReturnBadArgument;

    com_tsoniq_driver_DisplayXFBFramebuffer* device = indexToFramebuffer(displayIndex);
    if (!device) return kIOReturnNotFound;

    return device->userClientGetConfiguration(config);
}


/** Set the current display information.
 *
 *  @param  config          Supplies the display structure. On return, contains updated data.
 *  @param  displayIndex    The display index.
 *  @return                 An IOReturn code.
 */
IOReturn DisplayXFBDriver::userClientSetConfiguration(const DisplayXFBConfiguration* config, unsigned displayIndex)
{
    TSTrace();
    if (!config) return kIOReturnBadArgument;

    com_tsoniq_driver_DisplayXFBFramebuffer* device = indexToFramebuffer(displayIndex);
    if (!device) return kIOReturnNotFound;

    return device->userClientSetConfiguration(config);
}


/** Query the current display information.
 *
 *  @param  state           Returns the display structure.
 *  @param  displayIndex    The display index.
 *  @return                 An IOReturn code.
 */
IOReturn DisplayXFBDriver::userClientGetState(DisplayXFBState* state, uint32_t displayIndex)
{
    TSTrace();
    if (!state) return kIOReturnBadArgument;

    com_tsoniq_driver_DisplayXFBFramebuffer* device = indexToFramebuffer(displayIndex);
    if (!device) return kIOReturnNotFound;

    return device->userClientGetState(state);
}


/** Connect a display.
 *
 *  @param  displayIndex    The display index.
 *  @return                 An IOReturn code.
 */
IOReturn DisplayXFBDriver::userClientConnect(unsigned displayIndex)
{
    TSTrace();
    com_tsoniq_driver_DisplayXFBFramebuffer* device = indexToFramebuffer(displayIndex);
    if (!device) return kIOReturnNotFound;

    return device->userClientConnect();
}


/** Connect a display.
 *
 *  @param  displayIndex    The display index.
 *  @return                 An IOReturn code.
 */
IOReturn DisplayXFBDriver::userClientDisconnect(unsigned displayIndex)
{
    TSTrace();
    com_tsoniq_driver_DisplayXFBFramebuffer* device = indexToFramebuffer(displayIndex);
    if (!device) return kIOReturnNotFound;

    return device->userClientDisconnect();
}


/** Map the shared data in to a client's address space.
 *
 *  @param  readOnly        Logical true to create a read-only mapping.
 *  @param  task            The task to map the data to.
 *  @param  displayIndex    The display index.
 *  @param  mapType         The object to be mapped.
 *  @return                 An IOMemoryMap for the data, or zero if failed. Ownership of the object
 *                          is passed to the caller (releasing it will remove the mapping).
 */
IOMemoryMap* DisplayXFBDriver::userClientMapInTask(bool readOnly, task_t task, unsigned displayIndex, unsigned mapType)
{
    TSTrace();
    com_tsoniq_driver_DisplayXFBFramebuffer* device = indexToFramebuffer(displayIndex);
    return (device) ? device->userClientMapInTask(readOnly, task, mapType) : 0;
}



/** Check if a display index references a display.
 *
 *  @param  displayIndex    The display number.
 *  @return                 Logical true if the index maps to a display, false if no display matches the index.
 */
bool DisplayXFBDriver::validateDisplayIndex(unsigned displayIndex) const
{
    return 0 != indexToFramebuffer(displayIndex);
}


#pragma mark    -
#pragma mark    Framebuffer Services

/** Send a notification.
 *
 *  @param  code    The notification code (kDisplayXFBNotificationXyz).
 *  @param  fb      The framebuffer issuing the notification.
 */
void DisplayXFBDriver::sendNotification(uint32_t code, const com_tsoniq_driver_DisplayXFBFramebuffer* framebuffer)
{
    unsigned displayIndex = framebufferToIndex(framebuffer);
    if (displayIndex >= kDisplayXFBMaxDisplays)
    {
        TSLog("Invalid framebuffer");
    }
    else
    {
        switch (code)
        {
            case kDisplayXFBNotificationDisplayState:
            case kDisplayXFBNotificationCursorState:
            case kDisplayXFBNotificationCursorImage:
                messageClients(code, (void*)(uintptr_t)displayIndex);
                break;

            default:
                // Ignore: invalid code.
                TSLog("Invalid code");
                break;
        }
    }
}
