/** @file       DisplayXFBFramebuffer.h
 *  @brief      Virtual display driver for OSX.
 *  @copyright  Copyright (c) 2010 tsoniq. All Rights Reserved.
 */

#ifndef COM_TSONIQ_DisplayXFBFramebuffer_H
#define COM_TSONIQ_DisplayXFBFramebuffer_H

#include "DisplayXFBNames.h"
#include "DisplayXFBShared.h"
#include "DisplayXFBTiming.h"

#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/IOPlatformExpert.h>
//#include <IOKit/pci/IOPCIDevice.h>



/** The IOKit driver object for a virtual display driver.
 */
class DisplayXFBFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(DisplayXFBFramebuffer);

public:

    // Methods for IOService.

    virtual bool start(IOService *provider);
    virtual void stop(IOService *provider);

    // Methods for IOFramebuffer.

    virtual IOReturn enableController();
    virtual IODeviceMemory* getVRAMRange();
    virtual UInt32 getApertureSize(IODisplayModeID displayMode, IOIndex depth);
    virtual IODeviceMemory* getApertureRange(IOPixelAperture aperture);
    virtual IOItemCount getConnectionCount();

    virtual IOReturn setAttribute (IOSelect attribute, uintptr_t value);
    virtual IOReturn getAttribute (IOSelect attribute, uintptr_t* value);
    virtual IOReturn setAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t value);
    virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t* value);
    virtual const char* getPixelFormats();
    virtual IOItemCount getDisplayModeCount();
    virtual IOReturn getDisplayModes (IODisplayModeID* allDisplayModes);
    virtual IOReturn getInformationForDisplayMode (IODisplayModeID displayMode, IODisplayModeInformation* info);
    virtual UInt64 getPixelFormatsForDisplayMode (IODisplayModeID displayMode, IOIndex depth);
    virtual IOReturn getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation* info);
    virtual IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture, IOPixelInformation* pixelInfo);
    virtual IOReturn getCurrentDisplayMode (IODisplayModeID* displayMode, IOIndex* depth);
    virtual IOReturn getStartupDisplayMode(IODisplayModeID* displayMode, IOIndex* depth);
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth);
    virtual IOReturn setGammaTable(UInt32 channelCount, UInt32 dataCount, UInt32 dataWidth, void* data);
    virtual IOReturn registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc, OSObject* target, void* ref, void** interruptRef);
    virtual IOReturn unregisterInterrupt(void *interruptRef);
    virtual IOReturn setInterruptState(void* interruptRef, UInt32 state);
    virtual IOReturn setCursorImage(void* cursorImage);
    virtual IOReturn setCursorState(SInt32 x, SInt32 y, bool visible);


    virtual bool hasDDCConnect(IOIndex connectIndex);
    virtual IOReturn getDDCBlock(IOIndex connectIndex, UInt32 blockNumber, IOSelect blockType, IOOptionBits options, UInt8* data, IOByteCount* length);

    // Methods used for the user-client access.
    IOReturn userClientGetConfiguration(ts::DisplayXFBConfiguration* config);
    IOReturn userClientSetConfiguration(const ts::DisplayXFBConfiguration* config);
    IOReturn userClientGetState(ts::DisplayXFBState* state);
    IOReturn userClientConnect();
    IOReturn userClientDisconnect();
    IOMemoryMap* userClientMapInTask(bool readOnly, task_t task, unsigned mapType);


private:

    static const unsigned kFramePadding = 1024;             //! Rumour (incorrect?) has it that some padding bytes are needed at the end of the display

    struct InterruptHandler
    {
        IOFBInterruptProc m_handler;                        //! Interrupt callback on monitor connect/disconnect (or zero if none)
        OSObject* m_object;                                 //! Parameter for connection interrupt handler
        void* m_ref;                                        //! Parameter for connection interrupt handler
        bool m_enabled;                                     //! Logical true if enabled, false otherwise

        void init()
        {
            m_handler = 0;
            m_object = 0;
            m_ref = 0;
            m_enabled = false;
        }

        void clear()
        {
            init();
        }

        void assign(IOFBInterruptProc proc, OSObject* object, void* ref)
        {
            m_handler = proc;
            m_object = object;
            m_ref = ref;
            m_enabled = true;
        }

        bool isAssigned() const { return 0 != m_handler; }

        void setEnabled(bool enable) { m_enabled = enable; }
        bool isEnabled() const { return m_enabled; }

        void fire() const
        {
            if (isAssigned() && isEnabled()) m_handler(m_object, m_ref);
        }
    };

    DisplayXFBFramebuffer(const DisplayXFBFramebuffer&);            // Prevent copy constructor
    DisplayXFBFramebuffer& operator=(const DisplayXFBFramebuffer&); // Prevent assignment


    class com_tsoniq_driver_DisplayXFBDriver* m_provider;       //! The provider class
    unsigned m_displayIndex;                                    //! The display index number
    unsigned m_vramSize;                                        //! The maximum display width
    IOWorkLoop* m_vblankWorkLoop;                               //! Work loop used for vblank events
    IOTimerEventSource* m_vblankTimerEventSource;               //! Event source used to emulate vblank timing interrupts
    UInt32 m_vblankPeriodUS;                                    //! The vblank interval, in microseconds
    com_tsoniq_driver_DisplayXFBTiming m_vblankTiming;          //! Timing handler
    bool m_vblankTimerIsEnabled;                                //! Logical true if the timer is enabled
    IOBufferMemoryDescriptor* m_displayMemory;                  //! The framebuffer memory description (the raw RGBA32 pixel array)
    IOBufferMemoryDescriptor* m_cursorMemory;                   //! The cursor state (DisplayXFBCursor)
    ts::DisplayXFBCursor* m_cursor;                             //! The cursor referenced by m_cursorMemory
    InterruptHandler m_connectInterruptHandler;                 //! Handler for connect interrupts
    InterruptHandler m_vblankInterruptHandler;                  //! Handler for VBlank interrupts
    ts::DisplayXFBConfiguration m_configuration;                //! The current configuration
    ts::DisplayXFBState m_state;                                //! The current state

	static void vblankEventHandler(OSObject* owner, IOTimerEventSource* sender);
	void vblankEventEnable(bool enable);

	IOReturn sharedMemoryAlloc(IOBufferMemoryDescriptor** buffer, unsigned size, bool contiguous);
	void sharedMemoryFree(IOBufferMemoryDescriptor** buffer);
};

#endif  // COM_TSONIQ_DisplayXFBFramebuffer_H
