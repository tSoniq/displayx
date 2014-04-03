/** @file   DisplayXFBAccelerator.h
 *  @brief  IOAccelerator implementation.
 *
 *  Copyright (c) 2011 tSoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_DisplayXFBAccelerator_H
#define COM_TSONIQ_DisplayXFBAccelerator_H   (1)

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/graphics/IOGraphicsInterfaceTypes.h>
#include <IOKit/graphics/IOAccelerator.h>
#include "DisplayXFBNames.h"
#include "DisplayXFBFramebuffer.h"
#include "DisplayXFBShared.h"

/** The accelerator object. A single accelerator is shared between all framebuffers and it
 *  would normally provide kernel-level services to the user-mode GA plugin. In our case,
 *  it really exists only to allow the plugin to be loaded - any subsequent attempt to use
 *  the GA to create an accelerated surface (via a system user-client) will result in an
 *  error being returned to the caller.
 */
class DisplayXFBAccelerator : public IOAccelerator
{
    OSDeclareDefaultStructors(DisplayXFBAccelerator);

public:

    // IOService methods.

    virtual bool init(OSDictionary *dictionary = 0);
    virtual void free();
    virtual bool start(IOService* provider);
    virtual void stop(IOService* provider);

    // Local implementation.
    unsigned getAccelCaps() { return 0x03; }                                        //! Return the value for IOAccelCaps
    unsigned getAccelRevision() { return kCurrentGraphicsInterfaceRevision; }       //! Return the value for IOAccelRevision
    const char* getAccelTypes() { return m_types; }                                 //! Return the accelerator types string

private:

    char m_types[512];

    DisplayXFBAccelerator(const DisplayXFBAccelerator&);            // Prevent copy constructor
    DisplayXFBAccelerator& operator=(const DisplayXFBAccelerator&); // Prevent assignment
};


#define DisplayXFBAcceleratorStringify0(str) #str
#define DisplayXFBAcceleratorStringify1(str) DisplayXFBAcceleratorStringify0(str)
#define DisplayXFBAcceleratorClassName DisplayXFBAcceleratorStringify1(DisplayXFBAccelerator) // The mangled class name as c-string

#endif      // COM_TSONIQ_DisplayXFBAccelerator_H
