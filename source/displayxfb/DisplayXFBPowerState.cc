/** @file   com_tsoniq_driver_DisplayXFBPowerState.cc
 *  @brief  Power management states.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#include "DisplayXFBPowerState.h"

/** The power state structures for the driver.
 *  See IONDRVFramebuffer for an example.
 */
IOPMPowerState DisplayXFBDriverPowerStates[kDisplayXFBNumPowerStates] =
{
    // version, capabilityFlags, outputPowerCharacter, inputPowerRequirement (remaining fields unused)
    { 1, 0,                 0,              0,              0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 0,                 0,              IOPMPowerOn,    0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, IOPMDeviceUsable,  IOPMPowerOn,    IOPMPowerOn,    0, 0, 0, 0, 0, 0, 0, 0 }
};
