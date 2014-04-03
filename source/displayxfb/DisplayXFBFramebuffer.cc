/** @file       DisplayXFBFramebuffer.cc
 *  @brief      Virtual display driver for OSX.
 *  @copyright  Copyright (c) 2010 tsoniq. All Rights Reserved.
 */

#include "DisplayXFBFramebuffer.h"
#include "DisplayXFBDriver.h"
#include "DisplayXFBPowerState.h"

#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOSubMemoryDescriptor.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>
#include <IOKit/ndrvsupport/IOMacOSVideo.h>
#include <libkern/OSByteOrder.h>

using namespace ts;

// Console logging macro.
#if 0
#define TSLog(fmt, args...) do { IOLog("FB%p: %s: " fmt "\n", this, __FUNCTION__, ## args); } while (false)
#define TSTrace()           do { IOLog("FB%p: %s\n", this, __FUNCTION__); } while (false)
#else
#define TSLog(args...)      do { } while (false)
#define TSTrace()           do { } while (false)
#endif

#pragma mark    -
#pragma mark    EDID Handling

class DisplayXEDID
{
public:

    DisplayXEDID()
        :
        m_raw()
    {
        bzero(m_raw, sizeof m_raw);

        m_raw[0] = 0;       // Header
        m_raw[1] = 0xff;
        m_raw[2] = 0xff;
        m_raw[3] = 0xff;
        m_raw[4] = 0xff;
        m_raw[5] = 0xff;
        m_raw[6] = 0xff;
        m_raw[7] = 0;
        m_raw[8] = (uint8_t)((kDisplayXManufacturer >> 8) & 0xffu);
        m_raw[9] = (uint8_t)((kDisplayXManufacturer >> 0) & 0xffu);
        // 10-11 Product ID
        // 12-15 serial number
        // 16 week number
        m_raw[17] = 24;     // Year number (2010, stored as offset 1990)
        m_raw[18] = 0x01;   // EDID version (1.2)
        m_raw[19] = 0x02;
        m_raw[20] = 0x80;   // video flags
        m_raw[54] = 0;      // Block #1: pixel clock = 0
        m_raw[55] = 0;
        m_raw[56] = 0;      // Block #1: reserved
        m_raw[57] = 0xfc;   // Block #1: monitor name tag
        m_raw[58] = 0;      // Block #1: reserved
    }

    void setManufacturer(uint16_t mid)
    {
        m_raw[8] = (uint8_t)((mid >> 8) & 0xffu);
        m_raw[9] = (uint8_t)((mid >> 0) & 0xffu);
    }

    void setSerialNumber(uint32_t snum)
    {
        m_raw[12] = (uint8_t)((snum >>  0) & 0xffu);
        m_raw[13] = (uint8_t)((snum >>  8) & 0xffu);
        m_raw[14] = (uint8_t)((snum >> 16) & 0xffu);
        m_raw[15] = (uint8_t)((snum >> 24) & 0xffu);
    }

    void setBlock(const char* string, uint8_t type, unsigned index)
    {
        assert(index < 4);
        uint8_t* block = &m_raw[54 + (index * 18)];
        block[0] = 0;       // Pixel clock
        block[1] = 0;       //
        block[2] = 0;       // Reserved
        block[3] = type;    // Type code
        block[4] = 0;       // Reserved
        bool end = false;
        // Append string, truncating to 13 characters, terminating with 0x0a, padding with 0x20 (as per VESA specs)
        for (unsigned i = 0; i < 13; i++)
        {
            uint8_t code;
            if (end) code = 0x20;
            if (!end)
            {
                if (0 == string[i]) { end = true; code = 0x0a; }
                else { code = string[i]; }
            }
            block[5 + i] = code;
        }
    }

    void setChecksum()
    {
        unsigned sum = 0;
        for (unsigned i = 0; i < 127; i++) sum += m_raw[i];
        m_raw[127] = (uint8_t) (256 - (sum & 0xffu));
    }

    const uint8_t* rawData() const { return m_raw; }
    unsigned rawSize() const { return (unsigned)sizeof m_raw; }

private:

    uint8_t m_raw[128];
};




#pragma mark    -
#pragma mark    IOService Methods



#define super IOFramebuffer                                             //! Local superclass is IOFramebuffer
OSDefineMetaClassAndStructors(DisplayXFBFramebuffer, IOFramebuffer);    //! Create meta methods and con/destructors




/** Request to start the driver.
 */
bool DisplayXFBFramebuffer::start(IOService* provider)
{
    TSLog();

    // Start the super-class.
    if (! super::start(provider)) return false;


    // Set default member values (this is effectively the class constructor).
    m_provider = 0;
    m_displayIndex = 0;
    m_vramSize = 0;
    m_vblankWorkLoop = 0;
    m_vblankTimerEventSource = 0;
    m_vblankPeriodUS = 0;
    //m_vblankTiming;
    m_vblankTimerIsEnabled = false;
    m_displayMemory = 0;
    m_cursorMemory = 0;
    m_cursor = 0;
    m_connectInterruptHandler.init();
    m_vblankInterruptHandler.init();
    m_configuration.invalidate();
    m_state.invalidate();


    // Set a default configuration and state. Although strictly unnecessary, this ensures that
    // the configuration and state are always valid (so any !isValid() test denotes a coding problem).
    m_configuration.initialise("Default");
    m_configuration.appendMode(ts::kDisplayXFBDefaultWidth, ts::kDisplayXFBDefaultHeight, true);
    m_configuration.makeState(m_state, m_configuration.defaultModeIndex());


    // Get the provider.
    m_provider = OSDynamicCast(com_tsoniq_driver_DisplayXFBDriver, provider);
    if (!m_provider) { TSLog("No provider"); return false; }
    m_provider->retain();


    // Get our configuration.
    m_vramSize = m_provider->vramSize();
    m_displayIndex = m_provider->framebufferToIndex(this);
    TSLog("Framebuffer %u: vram size %u", m_displayIndex, m_vramSize);


    // Set up any registry keys that are necessary on the IOFramebuffer object.
    // Note that keys such as the CFPlugInTypes will be applied to the parent driver, not to this one. We
    // need to copy the keys here to ensure that IOAccelerator operations succeed.
    // This could also be generated this programatically.
    com_tsoniq_driver_DisplayXFBAccelerator* accel = m_provider->getAccelerator();
    if (!accel)
    {
        TSLog("No accelerator available");
    }
    else
    {
        OSDictionary* plugInDict = OSDynamicCast(OSDictionary, m_provider->getProperty("IOCFPlugInTypes"));
        if (!plugInDict) TSLog("No accelerator specified");
        else
        {
            // Set up the keys needed for IOAccelFindAccelerator() to function. See IOAccelSurfaceControl.c for
            // the usage of these keys by application layer code.
            setProperty("IOCFPlugInTypes", plugInDict);
            accel->setProperty("IOCFPlugInTypes", plugInDict);

            setProperty("IOAccelIndex", (unsigned long long)m_displayIndex, 32);
            setProperty("IOAccelRevision", accel->getAccelRevision());
            setProperty("IOAccelTypes", accel->getAccelTypes());
            TSLog("Set index %u, types %s", (unsigned)m_displayIndex, accel->getAccelTypes());
        }
    }

    // Set up the vblank handler. Don't treat any failure to set up the vblank timer as fatal, as the
    // the system can work without it (albeit without frame rate control). A zero m_vblankTimerEventSource
    // indicates that the timer will not be used.
    // To support this, we create a dedicated work loop here. IOFramebuffer overrides getWorkLoop() to do some
    // undocumented things and will return NULL if called at this point.
    m_vblankWorkLoop = IOWorkLoop::workLoop();
    if (!m_vblankWorkLoop) TSLog("no vblank workloop");
    else
    {
        m_vblankTimerEventSource = IOTimerEventSource::timerEventSource(this, (IOTimerEventSource::Action)&DisplayXFBFramebuffer::vblankEventHandler);
        if (!m_vblankTimerEventSource) TSLog("no vblank event source");
        else
        {
            IOReturn status = m_vblankWorkLoop->addEventSource(m_vblankTimerEventSource);
            if (kIOReturnSuccess != status)
            {
                TSLog("Error %08x adding vblank event source", (unsigned)status);
                m_vblankTimerEventSource->release();
                m_vblankTimerEventSource = 0;

            }
        }
    }

    // Initialisation successful.
    TSLog("successful start");
    return true;
}



/** Request to stop the driver.
 */
void DisplayXFBFramebuffer::stop(IOService* provider)
{
    TSLog("provider %p", (void*)provider);

    m_connectInterruptHandler.clear();
    m_vblankInterruptHandler.clear();

    if (m_vblankTimerEventSource)
    {
        vblankEventEnable(false);
        if (m_vblankWorkLoop) m_vblankWorkLoop->removeEventSource(m_vblankTimerEventSource);
        m_vblankTimerEventSource->release();
        m_vblankTimerEventSource = 0;
    }

    if (m_vblankWorkLoop)
    {
        m_vblankWorkLoop->release();
        m_vblankWorkLoop = 0;
    }

    if (m_provider)
    {
        m_provider->release();
        m_provider = 0;
    }

    m_cursor = 0;
    sharedMemoryFree(&m_cursorMemory);
    sharedMemoryFree(&m_displayMemory);

    super::stop(provider);
}





#pragma mark    -
#pragma mark    IOFramebuffer Methods



/** Perform hardware initialisation (IOFramebuffer).
 */
IOReturn DisplayXFBFramebuffer::enableController()
{
    TSTrace();

    // Allocate the frame buffer. Changing the allocation on-the-fly is not a good idea, due to
    // fragmentation. Instead, the maximum required allocation is made so that we can fail
    // cleanly if there is a problem. See Technical Q&A QA1197.
    IOReturn status = kIOReturnSuccess;
    do
    {
        // Allocate the VRAM.
        status = sharedMemoryAlloc(&m_displayMemory, m_vramSize, false);
        if (kIOReturnSuccess != status) break;


        // Allocate and initialise the cursor state description.
        status = sharedMemoryAlloc(&m_cursorMemory, (unsigned)sizeof (ts::DisplayXFBCursor), false);
        if (kIOReturnSuccess != status) break;

        m_cursor = (DisplayXFBCursor*)m_cursorMemory->getBytesNoCopy();
        if (!m_cursor) { status = kIOReturnNoMemory; break; }
        m_cursor->initialise();


        // Register the power management states
        // Do not need to call PMinit()/PMstop() as this is handled in the super-class.
        registerPowerDriver(this, DisplayXFBDriverPowerStates, kDisplayXFBNumPowerStates);
        changePowerStateTo(kDisplayXFBPowerStateWake);
        getProvider()->setProperty("IOPMIsPowerManaged", true);

    } while (false);

    // Clean up if there was an error.
    if (kIOReturnSuccess != status)
    {
        sharedMemoryFree(&m_cursorMemory);
        sharedMemoryFree(&m_displayMemory);
    }

    return status;
}





/** Return the video ram address information for the entire memory window of the card. Here we return
 *  the allocated memory window, since it appears that the super class wants this to be a superset of
 *  the mapping signalled by getApertureRange().
 */
IODeviceMemory* DisplayXFBFramebuffer::getVRAMRange()
{
    IODeviceMemory* memory = 0;

    if (!m_displayMemory)
    {
        TSLog("no buffer memory");
    }
    else
    {
        // REVIEW: This cast is less than ideal, but it avoids the need for genuinely physically contiguous allocation.
        memory = (IODeviceMemory*)m_displayMemory;
        memory->retain();
        TSLog("--> %p", memory);
    }

    return memory;
}


/** Return the aperture size.
 */
UInt32 DisplayXFBFramebuffer::getApertureSize(IODisplayModeID displayMode, IOIndex depth)
{
    unsigned modeIndex = ((unsigned)displayMode) - 1;
    DisplayXFBState state;

    if (!m_configuration.makeState(state, modeIndex) || depth != 0)
    {
        TSLog("bad ident %u", modeIndex);
        return 0;
    }
    else
    {
        TSLog("aperture size %u", (unsigned)state.bytesPerFrame());
        return (UInt32) state.bytesPerFrame();
    }
}


/** Return the memory window for access to the framebuffer.
 */
IODeviceMemory* DisplayXFBFramebuffer::getApertureRange(IOPixelAperture aperture)
{
    IODeviceMemory* memory;

    if (kIOFBSystemAperture != aperture)
    {
        TSLog("not system aperture");
        memory = 0;
    }
    else if (!m_state.isValid())
    {
        TSLog("invalid current display mode");
        memory = 0;
    }
    else
    {
        // REVIEW: dubious memory cast to avoid physically contiguous memory.
        memory = (IODeviceMemory*)IOSubMemoryDescriptor::withSubRange(
            m_displayMemory,                        // parent descriptor
            0,                                      // offset
            (IOByteCount)m_state.bytesPerFrame(),   // length
            (IOOptionBits)0);                       // options
        TSLog("--> %p", memory);
    }

    return memory;
}



/** Return the number of monitors that are connected. Only one monitor can be connected
 *  to a single IOFramebuffer instance at a time (despite what the framebuffer APIs might suggest).
 */
IOItemCount DisplayXFBFramebuffer::getConnectionCount()
{
    //TSTrace();
    return 1;       // MacOSX (and this driver) only support one display per IOFramebuffer instance.
}


/** Return a driver attribute.
 *
 *  @param  attribute       Identifies the attribute.
 *  @param  value           Returns the value.
 *  @return                 An IOKit status.
 */
IOReturn DisplayXFBFramebuffer::getAttribute(IOSelect attribute, uintptr_t* value)
{
    //TSLog("%c%c%c%c", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0]);

    IOReturn status = kIOReturnBadArgument;

    switch (attribute)
    {
        case kIOHardwareCursorAttribute:
            if (value)
            {
                // By returning logical true, we promise to implement setCursorImage() and setCursorState().
                *value = 1;
                status = kIOReturnSuccess;
            }
            break;

        default:
            status = super::getAttribute(attribute, value);
            break;
    }

    if (value) TSLog("%c%c%c%c --> %x %x", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0], (unsigned)*value, (unsigned)status);
    else TSLog("%c%c%c%c --> n/a %x", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0], (unsigned)status);
    return status;
}


/** Set an attribute on the driver.
 *
 *  @param  attribute       The selector.
 *  @param  value           The value to set.
 *  @return                 An IOKit status value.
 */
IOReturn DisplayXFBFramebuffer::setAttribute(IOSelect attribute, uintptr_t value)
{
    //TSLog("attribute = %c%c%c%c, value = %x", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0], (unsigned)value);

    // Apply the operation to the super-class.
    IOReturn status = super::setAttribute(attribute, value);

    switch (attribute)
    {
        case kIOPowerAttribute:                         // 'powr': value is the power state number (m_powerState)
            handleEvent((value > kDisplayXFBPowerStateOff) ? kIOFBNotifyWillPowerOn : kIOFBNotifyWillPowerOff);
            handleEvent((value > kDisplayXFBPowerStateOff) ? kIOFBNotifyDidPowerOn : kIOFBNotifyDidPowerOff);
            status = kIOReturnSuccess;
            break;

        /* Some drivers handle kIOMirrorAttribute here as well. This is passed a pointer to
         * another framebuffer driver. See IONDRVVFramebuffer for an example of handling.
         */

        default:                                        // Everything else already handled
            break;
    }

    TSLog("%c%c%c%c --> %x", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0], (unsigned)status);

    return status;
}



/** Return an attribute specific to a particular output connection.
 */
IOReturn DisplayXFBFramebuffer::getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value)
{
    IOReturn status;

    switch (attribute)
    {
        case kConnectionEnable:                 // 'enab'
        case kConnectionCheckEnable:            // 'cena'
            if (value) *value = m_state.isConnected() ? 1 : 0;
            status = kIOReturnSuccess;
            break;

        case kConnectionSupportsHLDDCSense:     // 'hddc'
            if (value) *value = m_state.isConnected() ? 1 : 0;
            status = kIOReturnSuccess;
            break;

        default:
            status = super::getAttributeForConnection(connectIndex, attribute, value);
            break;
    }

    TSLog("%c%c%c%c --> %x", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0], (unsigned)status);
    return status;
}


/** Return an attribute specific to a particular output connection.
 */
IOReturn DisplayXFBFramebuffer::setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value)
{
    // REVIEW: consider adding support for kConnectionSyncEnable (see NDRV example)
    // REVIEW: consider adding support for kConnectionFlags (see NDRV example)

    IOReturn status;
    switch (attribute)
    {
        case kConnectionPower:                      // 'powr'
            status = kIOReturnSuccess;              // Just return success - nothing to be done.
            break;

        case kConnectionProbe:                      // 'prob'
            m_connectInterruptHandler.fire();       // Trigger an immediate connect change interrupt on probe (for now)
            status = kIOReturnSuccess;
            break;

        default:
            status = super::setAttributeForConnection(connectIndex, attribute, value);
            break;
    }

    TSLog("%c%c%c%c --> %x", ((char*)&attribute)[3], ((char*)&attribute)[2], ((char*)&attribute)[1], ((char*)&attribute)[0], (unsigned)status);
    return status;
}



/** Return the pixel formats supported by the display.
 *
 *  @retun                  A string describing the available formats (!).
 */
const char* DisplayXFBFramebuffer::getPixelFormats()
{
    // Consider extending this to permit the use of 16 bit pixel formats (for speed).
    static const char* pixelFormats = IO32BitDirectPixels "\0\0";
    return pixelFormats;
}


/** Return the number of display modes.
 */
IOItemCount DisplayXFBFramebuffer::getDisplayModeCount()
{
    //TSLog("modeCount %u", (unsigned)m_configuration.modeCount());
    return m_configuration.modeCount();
}


/** Return the display modes.
 *
 *  @param  allDisplayModes     An array of length @e getDisplayModeCount() in to which the display mode IDs should be written.
 *  @return                     An IOKit status.
 */
IOReturn DisplayXFBFramebuffer::getDisplayModes(IODisplayModeID* allDisplayModes)
{
    TSLog("Returning indices for %u modes", m_configuration.modeCount());
    for (unsigned i = 0; i < m_configuration.modeCount(); i++)
    {
        allDisplayModes[i] = (i + 1);   // Use the mode index + 1 as the IOFramebuffer mode ident
    }
    return kIOReturnSuccess;
}


/** Return the display mode information.
 *
 *  @param  displayMode         The display mode to return data for.
 *  @param  info                Structure used to return the data.
 *  @return                     An IOKit status.
 */
IOReturn DisplayXFBFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation* info)
{
    TSLog("ID %u, info %p, modeCount %u", (unsigned)displayMode, info, (unsigned)m_configuration.modeCount());

    if (!info) return kIOReturnBadArgument;

    unsigned modeIndex = ((unsigned)displayMode) - 1;

    if (modeIndex >= m_configuration.modeCount()) return kIOReturnBadArgument;

    const DisplayXFBMode& mode = m_configuration.mode(modeIndex);
    bzero(info, sizeof (*info));
    info->nominalWidth = mode.width();
    info->nominalHeight = mode.height();
    info->refreshRate = m_configuration.refreshRate1616();
    info->maxDepthIndex = 0;
    info->flags = kDisplayModeSafeFlag | kDisplayModeValidFlag;
    if (m_configuration.defaultModeIndex() == modeIndex) info->flags |= (kDisplayModeDefaultFlag);

    TSLog("ID %u, %u x %u", (unsigned)displayMode, mode.width(), mode.height());
    return kIOReturnSuccess;
}


/** Return the available pixel formats. This is an obsolete method that should always return zero.
 *
 *  @param  displayMode         Unused
 *  @param  depth               Unused
 *  @return                     0
 */
UInt64 DisplayXFBFramebuffer::getPixelFormatsForDisplayMode (IODisplayModeID displayMode, IOIndex depth)
{
    (void)displayMode;
    (void)depth;
    return 0;
}


/** Return a description of the pixel format.
 *
 *  @param  displayMode         The mode to describe.
 *  @param  depth               The depth index for the mode.
 *  @param  aperture            The display aperture (memory window). Always kIOFBSystemAperture.
 *  @param  info                Returns the pixel format description.
 *  @return                     An IOKit status.
 */
IOReturn DisplayXFBFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation* info)
{
    TSLog("mode %u", (unsigned)displayMode);

    (void)aperture;

    if (!info) return kIOReturnBadArgument;
    if (depth != 0) return kIOReturnUnsupportedMode;

    DisplayXFBState state;
    if (!m_configuration.makeState(state, ((unsigned)displayMode) - 1)) return kIOReturnBadArgument;

    // Note: this code will need to change if support for 16 bit pixel formats is added
    bzero(info, sizeof (*info));
    info->bytesPerRow = state.bytesPerRow();
    info->bytesPerPlane = 0;                            // Not used
    info->bitsPerPixel = state.bitsPerPixel();          // Number of bits per pixel (including unused/alpha bits)
    info->pixelType = kIORGBDirectPixels;               // Pixel type encodes colour directly (ie not index CLUT).
    info->componentCount = 3;                           // Three components per pixel (RGB implied)
    info->bitsPerComponent = 8;                         // Bits per component
    info->componentMasks[0] = 0x00FF0000;               // Mask for red component (in uint32_t pixel data)
    info->componentMasks[1] = 0x0000FF00;               // Mask for green component (in uint32_t pixel data)
    info->componentMasks[2] = 0x000000FF;               // Mask for blue component (in uint32_t pixel data)
    memcpy(info->pixelFormat, IO32BitDirectPixels, strlen(IO32BitDirectPixels) + 1);   // Specifies the encoding of data in a pixel
    info->flags = 0;                                    // No flags
    info->activeWidth = state.width();                  // Number of visible horizontal pixels
    info->activeHeight = state.height();                // Number of visible vertical pixels

    TSLog("done");
    return kIOReturnSuccess;
}


/** Return the current display mode.
 *
 *  @param  displayMode     Returns the mode.
 *  @param  depth           Returns the depth.
 *  @return                 An IOKit status.
 */
IOReturn DisplayXFBFramebuffer::getCurrentDisplayMode(IODisplayModeID* displayMode, IOIndex* depth)
{
    if (!m_state.isValid())
    {
        // No valid current mode.
        if (displayMode) *displayMode = 0;
        if (depth) *depth = 0;
        TSLog("no mode");
        return kIOReturnError;
    }
    else
    {
        if (displayMode) *displayMode = (IODisplayModeID) (m_state.modeIndex() + 1);
        if (depth) *depth = 0;
        TSLog("mode %u", (unsigned)m_state.modeIndex());
        return kIOReturnSuccess;
    }
}


/** Return the startup display mode.
 *
 *  @param  displayMode     Returns the mode ident.
 *  @param  depth           Returns the depth.
 */
IOReturn DisplayXFBFramebuffer::getStartupDisplayMode(IODisplayModeID* displayMode, IOIndex* depth)
{
    TSTrace();
    if (displayMode) *displayMode = (IODisplayModeID)1;
    if (depth) *depth = 0;
    return kIOReturnSuccess;
}


/** Get timing data.
 *
 *  @param  displayMode     The display mode.
 *  @param  info            Returns the timing data.
 *  @return                 An IOKit status.
 */
IOReturn DisplayXFBFramebuffer::getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation* info)
{
    (void)displayMode;

    TSTrace();

    if (!info) return kIOReturnBadArgument;

    memset(info, 0, sizeof *info);                          // Clear all fields (see description in IOGraphicsTypes.h)
    info->appleTimingID = kIOTimingIDApple_FixedRateLCD;    // Use a preset template (it is unclear what this means for VBlank speed...)

    return kIOReturnSuccess;
}


/** Configure the output display mode.
 */
IOReturn DisplayXFBFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    unsigned modeIndex = ((unsigned)displayMode) - 1;
    if (0 == depth && modeIndex < m_configuration.modeCount())
    {
        m_state.setMode(m_configuration.mode(modeIndex), modeIndex);
        m_provider->sendNotification(kDisplayXFBNotificationDisplayState, this);
        TSLog("success : modeIndex %u", modeIndex);
        return kIOReturnSuccess;
    }
    else
    {
        TSLog("fail : modeIndex %u", modeIndex);
        return kIOReturnUnsupported;
    }
}


/** Set the gamma table.
 *
 *  @param  channelCount    The number of channels. If 1, then a single correction applies to all channels. If 3, then
 *                          distinct R, G and B corrections are supplied.
 *  @param  dataCount       The number of data items per channel.
 *  @param  dataWidth       The number of bits in each item. Either 8 or 16.
 *  @param  data            Packed array of correction data, in R, G, B order. It is unclear how the values should be used.
 *  @return                 An IOReturn status value.
 */
IOReturn DisplayXFBFramebuffer::setGammaTable(UInt32 channelCount, UInt32 dataCount, UInt32 dataWidth, void* data)
{
    //TSLog("%u/%u/%u/%p", (unsigned)channelCount, (unsigned)dataCount, (unsigned)dataWidth, data);
    // Add support for colour calibration here. This is really only viable if it can be done in hardware,
    // at the final display output (operating on each individual pixel is extremely slow).
    (void)channelCount;
    (void)dataCount;
    (void)dataWidth;
    (void)data;
    return kIOReturnSuccess;
}



/** Register an interrupt callback for VBlank or connection events.
 */
IOReturn DisplayXFBFramebuffer::registerForInterruptType(
    IOSelect interruptType,
    IOFBInterruptProc proc,
    OSObject* target,
    void* ref,
    void** interruptRef)
{
    IOReturn status;

    TSLog("%08x/%p/%p/%p/%p", (unsigned)interruptType, proc, target, ref, interruptRef);

    if (!proc || !interruptRef) return kIOReturnBadArgument;

    *interruptRef = 0;
    InterruptHandler* handler;
    if (interruptType == kIOFBConnectInterruptType)             handler = &m_connectInterruptHandler;
    else if (interruptType == kIOFBVBLInterruptType)            handler = &m_vblankInterruptHandler;
    else                                                        handler = 0;

    if (!handler) status = kIOReturnUnsupported;
    else if (handler->isAssigned()) status = kIOReturnBusy;
    else
    {
        handler->assign(proc, target, ref);
        handler->setEnabled(true);
        *interruptRef = (void*)handler;
        status = kIOReturnSuccess;
    }

    return status;
}


/** Deregister an interrupt handler.
 */
IOReturn DisplayXFBFramebuffer::unregisterInterrupt(void *interruptRef)
{
    IOReturn status;

    TSLog("ref %p", interruptRef);

    // The interrupt ref is the pointer that we assigned when the interrupt was registered.
    if (interruptRef == &m_vblankInterruptHandler || interruptRef == &m_connectInterruptHandler)
    {
        InterruptHandler* handler = (InterruptHandler*)interruptRef;
        handler->clear();
        status = kIOReturnSuccess;
    }
    else
    {
        status = kIOReturnUnsupported;
    }

    return status;
}


/** Enable or disable interrupt sources.
 *
 *  @param  interruptRef    A reference to the installed interrupt handler (as returned by registerForInterruptType()).
 *  @param  state           Non-zero to enable, zero to disable.
 */
IOReturn DisplayXFBFramebuffer::setInterruptState(void* interruptRef, UInt32 state)
{
    TSLog("%p/%u", interruptRef, (unsigned)state);

    if (!interruptRef) return kIOReturnBadArgument;
    else if (interruptRef == &m_connectInterruptHandler || interruptRef == &m_vblankInterruptHandler)
    {
        InterruptHandler* handler = (InterruptHandler*)interruptRef;
        handler->setEnabled((state != 0));
        return kIOReturnSuccess;
    }
    else
    {
        return super::setInterruptState(interruptRef, state);
    }
}


/** Signal that DDC information is available.
 */
bool DisplayXFBFramebuffer::hasDDCConnect(IOIndex connectIndex)
{
    TSTrace();
    (void)connectIndex;
    return true;
}


/** Get a DDC information block about the display.
 */
IOReturn DisplayXFBFramebuffer::getDDCBlock(
    IOIndex connectIndex,
    UInt32 blockNumber,
    IOSelect blockType,
    IOOptionBits options,
    UInt8* data,
    IOByteCount* length)
{
    TSTrace();

    (void)connectIndex;
    (void)blockNumber;
    (void)blockType;
    (void)options;

    if (!data) return kIOReturnBadArgument;

    DisplayXEDID edid;

    edid.setManufacturer(kDisplayXManufacturer);
    edid.setSerialNumber(m_displayIndex);
    edid.setBlock("DisplayX", 0xfc, 0);
    edid.setChecksum();

    memcpy(data, edid.rawData(), edid.rawSize());
    *length = edid.rawSize();

    return kIOReturnSuccess;
}


    /** Set the display image for the cursor.
     *
     *  @param  cursorImage     The new image data.
     *  @return                 An IOStatus return.
     */
    IOReturn DisplayXFBFramebuffer::setCursorImage(void* cursorImage)
    {
        // Create a description of the hardware format that we support (just ARGB32 in our case).
        IOHardwareCursorDescriptor description;
        memset(&description, 0, sizeof description);
        description.majorVersion = kHardwareCursorDescriptorMajorVersion;
        description.minorVersion = kHardwareCursorDescriptorMinorVersion;
        description.height = DisplayXFBCursor::kMaxHeight;
        description.width = DisplayXFBCursor::kMaxWidth;
        description.bitDepth = kIO32ARGBPixelFormat;
        description.numColors = 0;
        description.colorEncodings = 0;
        description.flags = 0;
        description.supportedSpecialEncodings = 0;

        // Create a description of the cursor data.
        IOHardwareCursorInfo info;
        memset(&info, 0, sizeof info);
        info.majorVersion = kHardwareCursorInfoMajorVersion;        // Structure version
        info.minorVersion = kHardwareCursorInfoMinorVersion;        //
        info.cursorHeight = 0;                                      // Returns the actual cursor height
        info.cursorWidth = 0;                                       // Returns the actual cursor width
        info.colorMap = 0;                                          // No indexed colours
        info.hardwareCursorData = (UInt8*)m_cursor->m_pixelData;    // Pointer to the memory to receive the image
        info.cursorHotSpotX = 0;                                    // Returns the host spot position
        info.cursorHotSpotY = 0;                                    // Returns the host spot position

        // Convert the cursor data.
        bool ok = convertCursorImage(cursorImage, &description, &info);
        IOReturn status;
        if (!ok)
        {
            m_cursor->m_isValid = 0;
            status = kIOReturnUnsupported;
            static bool didWarn = false;
            if (!didWarn)
            {
                didWarn = true;
                IOLog("DisplayX: convertCursorImage failed\n");
            }
        }
        else
        {
            m_cursor->m_hotspotX = info.cursorHotSpotX;
            m_cursor->m_hotspotY = info.cursorHotSpotY;
            m_cursor->m_width = info.cursorWidth;
            m_cursor->m_height = info.cursorHeight;
            m_cursor->m_isValid = 1;
            m_provider->sendNotification(kDisplayXFBNotificationCursorImage, this);
            status = kIOReturnSuccess;
        }
        return status;

    }


    /** Here to update the cursor state.
     *
     *  @param  x           The cursor x-position.
     *  @param  y           The cursor y-position.
     *  @param  visible     Logical true if the cursor is visible.
     *  @return             An IOStatus return.
     */
    IOReturn DisplayXFBFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible)
    {
        m_cursor->m_x = x;
        m_cursor->m_y = y;
        m_cursor->m_isVisible = (visible) ? 1 : 0;
        m_provider->sendNotification(kDisplayXFBNotificationCursorState, this);
        return kIOReturnSuccess;
    }


#pragma mark    -
#pragma mark    Private API



/** Enable or disable vblank timer callbacks. This method will do nothing if a vblank timer
 *  source is not available.
 *
 *  @param  enable      Logical true to enable callbacks, false to disable.
 */
void DisplayXFBFramebuffer::vblankEventEnable(bool enable)
{
    TSLog("enable %d", (int)enable);
    if (m_vblankWorkLoop && m_vblankTimerEventSource && enable != m_vblankTimerIsEnabled)
    {
        m_vblankTimerIsEnabled = enable;
        if (enable)
        {
            m_vblankPeriodUS = m_configuration.refreshPeriodUS();
            if (m_vblankPeriodUS < 1000) m_vblankPeriodUS = 1000;         // Safety limit
            if (m_vblankPeriodUS > 1000000) m_vblankPeriodUS = 1000000;   // Safety limit
            TSLog("VBlank enabled with period %u, rate %08x", (unsigned)m_vblankPeriodUS, m_configuration.refreshRate1616());
            m_vblankTiming.start(m_vblankPeriodUS);
            m_vblankTimerEventSource->setTimeoutUS(m_vblankPeriodUS);   // Kick off a chain of timer calls
        }
        else
        {
            m_vblankTimerEventSource->cancelTimeout();                  // Cancel any pending timer calls
        }
    }
}



/** Helper method: allocate memory suitable for sharing with a client application.
 *
 *  @param  buffer      Returns the buffer memory descriptor. Use getBytesNoCopy() to access the memory directly.
 *  @param  size        The allocation size (bytes).
 *  @param  contiguous  Specify logical true if a physically contiguous mapping is required.
 *  @return             A status code.
 */
IOReturn DisplayXFBFramebuffer::sharedMemoryAlloc(IOBufferMemoryDescriptor** buffer, unsigned size, bool contiguous)
{
    // Allocate the frame buffer. Changing the allocation on-the-fly is not a good idea, due to
    // fragmentation. Instead, the maximum required allocation is made so that we can fail
    // cleanly if there is a problem. See Technical Q&A QA1197.
    // Note: the display memory is allocated as contiguous and resident to permit the mapping
    // of the data as an IODeviceMemory (in case the OS makes assumptions about physical addresses).
    IOOptionBits options = kIOMemoryKernelUserShared;
    if (contiguous) options |= kIOMemoryPhysicallyContiguous;
    *buffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, options, size, PAGE_SIZE);
    if (!*buffer)
    {
        TSLog("failed to allocate buffer memory");
        return kIOReturnNoMemory;
    }
    else
    {
        return kIOReturnSuccess;
    }
}


/** Helper method: free memory allocated by sharedMemoryAlloc().
 *
 *  @param  buffer      Pointer to the buffer memory descriptor. Does nothing if *buffer is zero.
 *  @return             A status code.
 */
void DisplayXFBFramebuffer::sharedMemoryFree(IOBufferMemoryDescriptor** buffer)
{
    if (*buffer)
    {
        (*buffer)->release();
        *buffer = 0;
    }
}



#pragma mark    -
#pragma mark    User-client Methods


/** Return the current display configuration.
 *
 *  @param  config      Returns the current configuration data.
 *  @return             An status value.
 */
IOReturn DisplayXFBFramebuffer::userClientGetConfiguration(DisplayXFBConfiguration* config)
{
    TSTrace();
    if (!config) return kIOReturnBadArgument;
    *config = m_configuration;
    return kIOReturnSuccess;
}


/** Set the current configuration.
 *
 *  @param  config      Supplies and returns the current configuration data.
 *  @return             An status value.
 */
IOReturn DisplayXFBFramebuffer::userClientSetConfiguration(const DisplayXFBConfiguration* config)
{
    TSTrace();
    if (!config) return kIOReturnBadArgument;                       // Missing config
    if (!config->isValid()) return kIOReturnBadArgument;            // Invalid config object
    if (0 == config->modeCount()) return kIOReturnBadArgument;      // Need at least one mode to be defined...
    if (m_state.isConnected()) return kIOReturnBusy;                // Can't change the mode while the display is connected

    // Loop through the configuration to confirm that all requested modes can be supported.
    // We do this by briefly creating a state object for each mode and checking that there
    // is sufficient video memory to support the mode. If any mode is not usable, the whole
    // configuration is rejected.
    for (unsigned i = 0; i < config->modeCount(); i++)
    {
        const DisplayXFBMode& mode = config->mode(i);
        if (mode.width() < kDisplayXFBMinWidth || mode.height() < kDisplayXFBMinHeight) return kIOReturnBadArgument;    // Implausibly small size

        DisplayXFBState state;
        if (!config->makeState(state, i)) return kIOReturnBadArgument;
        if ((state.bytesPerFrame() + kFramePadding) > m_vramSize) return kIOReturnNoMemory;
    }

    // Looks plausible.
    m_configuration = *config;
    m_state.setMode(m_configuration.defaultMode(), m_configuration.defaultModeIndex());

    return kIOReturnSuccess;
}


/** Return the current display state.
 *
 *  @param  state       Returns the current state data.
 *  @return             An status value.
 */
IOReturn DisplayXFBFramebuffer::userClientGetState(DisplayXFBState* state)
{
    //TSTrace();
    if (!state) return kIOReturnBadArgument;
    *state = m_state;
    return m_state.isValid() ? kIOReturnSuccess : kIOReturnOffline;
}


/** Handle connect requests, simulating a plug-in of a new monitor.
 *
 *  @return                 An IO status return.
 *                          kIOReturnNotPermitted => display was already connected and can not be changed
 *                          kIOReturnUnsupportedMode => invalid configuration data
 *                          kIOReturnBadArgument => requested connection can not be made (invalid mode requested)
 *
 *  @note   This will start the display in the last used mode unless a preceding set-configuration request
 *          has been issued (which implicitly resets the default mode).
 *
 */
IOReturn DisplayXFBFramebuffer::userClientConnect()
{
    TSTrace();
    if (m_state.isConnected()) { return kIOReturnNotPermitted; }
    else if (!m_configuration.isValid() || 0 == m_configuration.modeCount()) return kIOReturnUnsupportedMode;
    else
    {
        m_configuration.makeState(m_state, m_configuration.defaultModeIndex());
        m_state.setIsConnected(true);
        vblankEventEnable(true);
        m_connectInterruptHandler.fire();
        m_provider->sendNotification(kDisplayXFBNotificationDisplayState, this);
        return kIOReturnSuccess;
    }
}


/** Handle disconnect requests, simulating a plug-in of a new monitor.
 *
 *  @return                 An IO status return.
 *                          kIOReturnNotPermitted => display was already disconnected.
 */
IOReturn DisplayXFBFramebuffer::userClientDisconnect()
{
    TSTrace();
    if (m_state.isConnected())
    {
        m_state.setIsConnected(false);
        vblankEventEnable(false);
        m_provider->sendNotification(kDisplayXFBNotificationDisplayState, this);
        m_connectInterruptHandler.fire();
    }
    return kIOReturnSuccess;
}


/** Map shared data for the display in to a task.
 *
 *  @param  readOnly        Logical true to create a read-only mapping.
 *  @param  task            The task to create a mapping in.
 *  @param  mapType         The object type to be mapped.
 *  @return                 A pointer to the memory mapping, or zero if failed.
 *                          Ownership of the returned object is passed to the caller.
 */
IOMemoryMap* DisplayXFBFramebuffer::userClientMapInTask(bool readOnly, task_t task, unsigned mapType)
{
    TSTrace();
    IOOptionBits options = kIOMapAnywhere;
    if (readOnly) options |= kIOMapReadOnly;
    IOBufferMemoryDescriptor* mem;
    switch (mapType)
    {
        case kDisplayXFBMapTypeDisplay:         mem = m_displayMemory;      break;
        case kDisplayXFBMapTypeCursor:          mem = m_cursorMemory;       break;
        default:                                mem = 0;                    break;
    }
    return (mem) ? (mem->createMappingInTask(task, 0, options)) : 0;
}




#pragma mark    -
#pragma mark    Callbacks





/** Timer callback, used to simulate vblank.
 *
 *  @param  owner   A pointer to the framebuffer object.
 *  @param  sender  The sending event source.
 */
void DisplayXFBFramebuffer::vblankEventHandler(OSObject* owner, IOTimerEventSource* sender)
{
    (void)sender;
    DisplayXFBFramebuffer* framebuffer = OSDynamicCast(DisplayXFBFramebuffer, owner);
    if (framebuffer)
    {
        if (framebuffer->m_vblankTimerIsEnabled)
        {
            uint64_t ticks = 0;
            uint32_t timeToNextTick = 0;
            framebuffer->m_vblankTiming.update(ticks, timeToNextTick);
            if (timeToNextTick < 100)
            {
                // Less than 0.1ms to next tick - trigger immediately and delay for 1 full tick
                framebuffer->m_vblankTimerEventSource->setTimeoutUS(framebuffer->m_vblankPeriodUS);
                framebuffer->m_vblankInterruptHandler.fire();               // Trigger the vblank interrupt(s)
            }
            else
            {
                framebuffer->m_vblankTimerEventSource->setTimeoutUS(timeToNextTick);
            }

            // Don't fire all possible missed interrupts - not helpful.
            for (uint64_t i = 0; i < ticks && i < 3; i++)
            {
                framebuffer->m_vblankInterruptHandler.fire();               // Trigger the vblank interrupt(s)
            }
        }


#if 0   // DEBUG frame timing (use the console logs to determine effective frame rate)
        static int count = 0;
        static int did = 0;
        count ++;
        if (framebuffer->m_vblankInterruptHandler.isEnabled()) did ++;
        if (0 == (count % 2048)) IOLog( "%s: In vblank timer callback (%d/%d:%d)\n", framebuffer->getName(), count, did, (int)framebuffer->m_vblankTimerIsEnabled);
#endif
    }
}

