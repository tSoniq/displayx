/** @file   DisplayXFBDriver.h
 *  @brief  The driver object to which the user-client is bound.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_DisplayXFBDriver_H
#define COM_TSONIQ_DisplayXFBDriver_H   (1)

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include "DisplayXFBNames.h"
#include "DisplayXFBFramebuffer.h"
#include "DisplayXFBShared.h"
#include "DisplayXFBAccelerator.h"

class DisplayXFBDriver : public IOService
{
    OSDeclareDefaultStructors(DisplayXFBDriver);

public:

    // IOService methods.

    virtual bool init(OSDictionary *dictionary = 0);
    virtual void free();
    virtual IOService* probe(IOService* provider, SInt32* score);
    virtual bool start(IOService* provider);
    virtual void stop(IOService* provider);
    virtual IOReturn setPowerState(unsigned long whichState, IOService* whatDriver);

    virtual bool handleOpen(IOService* forClient, IOOptionBits options, void* arg); // IOService override needed for multiple clients
    virtual void handleClose(IOService* forClient, IOOptionBits options);           // IOService override needed for multiple clients
    virtual bool handleIsOpen(const IOService* forClient) const;                    // IOService override needed for multiple clients

    // User client and support methods. Mostly these just re-direct to the frame buffer methods.
    IOReturn userClientOpen(ts::DisplayXFBInfo* info);
    IOReturn userClientClose();
    IOReturn userClientGetConfiguration(ts::DisplayXFBConfiguration* config, unsigned displayIndex);
    IOReturn userClientSetConfiguration(const ts::DisplayXFBConfiguration* config, unsigned displayIndex);
    IOReturn userClientGetState(ts::DisplayXFBState* state, uint32_t displayIndex);
    IOReturn userClientConnect(unsigned displayIndex);
    IOReturn userClientDisconnect(unsigned displayIndex);
    IOMemoryMap* userClientMapInTask(bool readOnly, task_t task, unsigned displayIndex, unsigned mapType);
    bool validateDisplayIndex(unsigned displayIndex) const;

    // Methods that are used from com_tsoniq_driver_DisplayXFBFramebuffer.
    unsigned vramSize() const { return m_vramSize; }            //! Return the VRAM size (bytes)
    unsigned framebufferToIndex(const com_tsoniq_driver_DisplayXFBFramebuffer* framebuffer) const;
    void sendNotification(uint32_t code, const com_tsoniq_driver_DisplayXFBFramebuffer* framebuffer);
    com_tsoniq_driver_DisplayXFBAccelerator* getAccelerator() { return m_accelerator; } //! Return the accelerator handle, or zero if not available

private:

    unsigned m_displayCount;                                                //! The number of display nubs that were created
    unsigned m_vramSize;                                                    //! The VRAM size (bytes)
    char m_displayName[32];                                                 //! The display name
    com_tsoniq_driver_DisplayXFBAccelerator* m_accelerator;                 //! The accelerator, or zero if not available
    com_tsoniq_driver_DisplayXFBFramebuffer* m_framebuffers[ts::kDisplayXFBMaxDisplays];  //! Array of one frame buffer per display
    IOService* m_clients[ts::kDisplayXFBMaxClients];                        //! Array of currently attached user-clients

    com_tsoniq_driver_DisplayXFBFramebuffer* indexToFramebuffer(unsigned displayIndex) const;
    bool getPropertyU32(unsigned* value, const char* key, unsigned minValue, unsigned maxValue, unsigned defValue) const;
    bool getPropertyStr(char* str, size_t size, const char* key, const char* defValue) const;

    DisplayXFBDriver(const DisplayXFBDriver&);              // Prevent copy constructor
    DisplayXFBDriver& operator=(const DisplayXFBDriver&);   // Prevent assignment
};

#endif      // COM_TSONIQ_DisplayXFBDriver_H
